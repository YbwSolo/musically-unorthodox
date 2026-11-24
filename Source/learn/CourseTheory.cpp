#include "Course.h"

// Music theory, in the order a beat maker needs it: time first, then notes, scales, chords and melody.
// Every example is in the plugin: the keyboard, the grid and the piano roll can all be clicked and heard.
namespace mu::course
{
using S = Scene;

std::vector<Lesson> theoryLessons()
{
    return {
    { "th.beat", "Time", "Beat, bar and BPM", {
        { "The beat is the pulse",
          "Every song has a steady pulse you can nod to. That pulse is the beat. Press play and count along out loud: one, two, three, four. Nearly all rap, drill, R&B, afrobeats and pop counts in fours.", S::Pulse, 0 },
        { "BPM is how fast the pulse is",
          "BPM means beats per minute. At 60 BPM there is one beat every second. At 120 BPM there are two. Drag the tempo and listen to how the same count turns from slow and heavy to urgent. The tempo sets the mood before a single note is played.", S::Pulse, 1 },
        { "Four beats make a bar",
          "Beats are grouped in fours, and each group is a bar. The first beat of a bar, the one, is the strongest. Music is built in bars: a loop is usually 2, 4 or 8 bars, a verse is usually 16, a hook is usually 8. When a producer says \"bring the drums in after four\", they mean four bars.", S::Pulse, 2 },
        { "Each beat splits into four steps",
          "One beat is too coarse to write drums on, so each beat is cut into four equal steps called sixteenth notes. That gives 16 steps in a bar. This is the grid in every drum machine, FL Studio's channel rack and Logic's step sequencer. The steps are counted \"1 e and a, 2 e and a\".", S::Grid, 0 },
        { "Typical tempos",
          "Styles live in tempo ranges. Knowing them tells you where to set your project before you start. These are guides, not rules: the right tempo is the one where the idea in your head feels right when you say the flow over it.", S::Card, 8 } },
      nullptr, kTabNone, nullptr },

    { "th.count", "Time", "Count the BPM yourself", {
        { "Find the pulse first",
          "Play the song and tap your foot or nod. Do not follow the hi-hats, they move too fast. Follow the snare or clap: in most songs it lands on beats two and four. If you can say \"one, two, three, four\" and the snare keeps landing on two and four, you have found the beat.", S::Grid, 2 },
        { "Count for fifteen seconds",
          "Now count every beat for exactly 15 seconds, then multiply by four. 23 beats in 15 seconds is about 92 BPM. Press start: a beat plays, the timer runs, and you count. When it stops, check your answer against the real tempo. Each try picks a different tempo.", S::Pulse, 3 },
        { "Tap it",
          "Faster than counting: tap along and let the gaps between your taps do the sum. Press start, then click the pad on every beat. After a few taps your tempo appears. It works without the practice beat too: play any song and tap along. This is exactly what the tap tempo button in a DAW does.", S::Pulse, 4 },
        { "Half time and double time",
          "Trap and drill are the trap for beginners. The hi-hats race, but the snare only lands once per bar, so it feels slow. Producers set the project to 140 and count it fast. A listener nods at 70. Both are right: one is double the other. If your count comes out at 70 and the hats are flying, the project tempo is 140.", S::Pulse, 5 },
        { "Check yourself",
          "Three checks. If your number is far outside the style's range, halve or double it. If you tapped to a whole verse and drifted, tap again for eight beats only. And when you set the DAW to your number, the metronome should stay locked to the song from the first bar to the last. If it slides away, you are a beat or two out.", S::Card, 8 } },
      nullptr, kTabNone, nullptr },

    { "th.lengths", "Time", "Note lengths and rhythm", {
        { "How long a note lasts",
          "Note lengths are named after how much of a bar they fill. A whole note fills the bar. A quarter note is one beat. An eighth note is half a beat, and a sixteenth is one step of the grid. Click each row to hear the same sound played as quarters, eighths and sixteenths.", S::Grid, 1 },
        { "On the beat and off the beat",
          "Steps 1, 5, 9 and 13 are the beats themselves. Hits there feel solid and expected. Hits between the beats are called off-beats, and they are what makes a rhythm move. A pattern with everything on the beat sounds like a march. Shift one kick off the beat and it starts to bounce.", S::Grid, 2 },
        { "Swing",
          "Play every step dead on the grid and it sounds like a machine. Swing delays every second step slightly, so the rhythm leans back and grooves. Boom bap lives on it. This pattern has swing turned up: listen to the hi-hats, they are not evenly spaced.", S::Grid, 3 },
        { "Triplets",
          "A triplet squeezes three notes into the space of two. It rolls instead of marching, and it is the sound of trap hi-hat rolls and of the triplet flow rappers use. In this bar the first two beats are straight and the last two are triplets: hear the hats start to roll.", S::Grid, 8 },
        { "Rests are part of the rhythm",
          "Silence is a decision. A gap before the snare makes the snare hit harder. A melody that never stops leaves no room for a vocal. When a pattern feels crowded, do not add anything: take a hit away and listen again.", S::Grid, 6 } },
      nullptr, kTabNone, nullptr },

    { "th.notes", "Notes", "The twelve notes", {
        { "There are only twelve",
          "All of Western music uses twelve notes, then repeats them higher and lower. The white keys are named A to G. The black keys sit between them and take their name from a neighbour: the black key above C is C sharp (C#), which is the same key as D flat (Db). Click any key to hear it.", S::Keys, 0 },
        { "Octaves",
          "Go up twelve keys from any note and you land on the same note again, higher. That distance is an octave, and the higher note vibrates exactly twice as fast. The same melody played an octave up or down is still the same melody. Producers use this all the time to make a part brighter or heavier.", S::Keys, 1 },
        { "Semitones and tones",
          "The distance from one key to the very next key, black or white, is a semitone. Two semitones make a tone. Notice there is no black key between E and F or between B and C: those pairs are already a semitone apart. Every scale and chord is just a recipe of tones and semitones.", S::Keys, 2 },
        { "How to find any note",
          "The black keys come in groups of two and three. C is always the white key just left of a group of two. F is just left of a group of three. Find those and you can name every key on any keyboard or piano roll, in any octave.", S::Keys, 0 } },
      nullptr, kTabNone, nullptr },

    { "th.scales", "Notes", "Keys and scales", {
        { "A key is a family of notes",
          "Songs do not use all twelve notes. They pick seven that belong together, called a scale, and treat one of them as home. Home is the root. The key of a song is its root plus its scale: \"C minor\" means home is C and the seven notes come from the minor recipe.", S::Keys, 4 },
        { "The major scale",
          "Major sounds bright and settled. Its recipe from the root is tone, tone, semitone, tone, tone, tone, semitone. Start on C and you get only white keys. Pick any other root above the keyboard and the same recipe gives that key's seven notes.", S::Keys, 3 },
        { "The minor scale",
          "Minor sounds dark, serious or sad, and it is the sound of most rap, drill and trap. Its recipe is tone, semitone, tone, tone, semitone, tone, tone. Pick a root and listen. If a beat you love sounds moody, it is almost certainly in a minor key.", S::Keys, 4 },
        { "Relatives share the same notes",
          "C major and A minor use exactly the same seven notes. What changes is which note feels like home. Every major key has a relative minor three semitones below it. This is why a sample or a loop can sit in two keys at once: play it over a different bass note and the mood flips.", S::Keys, 11 },
        { "What works and what does not",
          "Notes inside the key always sit together. Notes outside it sound wrong against the rest unless you pass through them quickly. A low note is holding here: click the yellow keys, then click a dim one and hear it rub. That clash is the sound of a wrong note. Staying in key is the single biggest step from random notes to music.", S::Keys, 5 },
        { "The five safe notes",
          "Drop two notes from the minor scale and you get the minor pentatonic: five notes with no semitone between any of them, so nothing in it can clash. Any order, any rhythm, and it sounds like a melody. When you are stuck, write with these five.", S::Keys, 6 },
        { "Your turn",
          "A key is named above the keyboard. Click its seven notes. Use the recipe: tone, semitone, tone, tone, semitone, tone, tone for minor. Right notes turn yellow, wrong ones flash red. Get all seven and it gives you another key.", S::Keys, 9 } },
      nullptr, kTabNone, nullptr },

    { "th.scales2", "Notes", "More colours", {
        { "Harmonic minor",
          "Raise the seventh note of the minor scale by a semitone and you get the harmonic minor. That one change creates a wide, tense gap near the top of the scale. It is the dramatic, cinematic sound in a lot of dark trap and drill melodies.", S::Keys, 12 },
        { "Phrygian",
          "Lower the second note of the minor scale by a semitone and you get Phrygian. The note sitting a semitone above home makes everything sound menacing. Drill and dark trap lean on it constantly: home, then the note right above it, then back.", S::Keys, 13 },
        { "Dorian and the blues scale",
          "Dorian is minor with a raised sixth: still minor, but warmer and more soulful, common in R&B and neo-soul. The blues scale is the minor pentatonic with one extra passing note. Choose each in the scale picker and compare them on the same root.", S::Keys, 14 },
        { "Finding the key of a sample or loop",
          "Loop the sound. Hum the note that feels like the end of the phrase, the one where it could stop. Find that note on the keyboard: it is usually the root. Then try the minor scale from it, then the major. Whichever fits without a clashing note is the key. Many sample packs also print the key in the file name.", S::Keys, 10 } },
      nullptr, kTabNone, nullptr },

    { "th.chords", "Chords", "Building chords", {
        { "A chord is notes played together",
          "Three or more notes at once make a chord. The basic chord, a triad, is built by picking a note of the scale, skipping one, taking the next, skipping one, and taking the next. Play, skip, play, skip, play. The lowest note names the chord.", S::Keys, 7 },
        { "Major and minor chords",
          "The middle note decides the mood. Four semitones above the root gives a major chord: bright. Three gives a minor chord: dark. Switch between them and watch only the middle note move, by one key.", S::Keys, 8 },
        { "The seven chords of a key",
          "Build a triad on each of the seven notes and you get the seven chords of the key. In a minor key the chords on notes 1, 4 and 5 are minor, the chords on 3, 6 and 7 are major, and the chord on 2 is diminished: tense, and used rarely. Click the numbers to hear each. Every one of them fits your key.", S::Keys, 7 },
        { "Roman numerals",
          "Producers name chords by their position so the idea works in any key. Lower case is minor, upper case is major. In C minor, \"i, VI, VII\" means C minor, A flat major, B flat major. In F minor the same numbers give F minor, D flat major, E flat major. Same feeling, different key.", S::Keys, 7 },
        { "Spreading a chord out",
          "The three notes do not have to be stacked tightly. Move the middle note up an octave and the chord opens up. Put the root an octave lower in the bass. Changing which note is at the bottom is called an inversion: it lets you move between chords with the smallest finger movements, which sounds smoother.", S::Roll, 9 } },
      nullptr, kTabNone, nullptr },

    { "th.prog", "Chords", "Chord progressions", {
        { "A progression is a journey",
          "A row of chords that repeats is a progression. It leaves home, creates tension, and comes back. Most beats use two to four chords, one per bar or one every two bars. This one is i, VI, III, VII in C minor: listen to it leave home and get pulled back.", S::Roll, 9 },
        { "Progressions that always work",
          "In a minor key: i to VI to VII is the big, triumphant trap sound. i to iv is dark and still. i to VI to III to VII is emotional and fits R&B and melodic rap. i to VII to VI walks downward and feels sad. Start with one of these, then change one chord and listen.", S::Card, 9 },
        { "One or two chords is enough",
          "Drill and a lot of trap sit on one chord for the whole beat, or rock between two. The movement comes from the melody, the 808 and the drums instead. Do not add chords because you think you should. Add them when the beat feels like it is standing still.", S::Roll, 11 },
        { "Arpeggios",
          "Play the notes of a chord one at a time instead of together and you have an arpeggio. It gives movement without adding a single new note, and it cannot clash because every note is in the chord. Bells, plucks and pianos in trap are very often arpeggios.", S::Roll, 10 } },
      nullptr, kTabNone, nullptr },

    { "th.melody", "Melody", "What makes a melody", {
        { "Start with a small idea",
          "A melody is not a long string of notes. It starts as a motif: three to five notes with a rhythm you can remember after one listen. If you cannot hum it back, it is too complicated. Press play: this is one bar, four notes.", S::Roll, 0 },
        { "Repeat it",
          "The ear likes what it recognises. Play the motif again and the listener starts to own it. Beginners avoid repeating because it feels lazy. It is the opposite: repetition is what makes a melody stick. Most hit melodies are one idea played many times.", S::Roll, 1 },
        { "Then change the ending",
          "Repeat exactly and it gets boring. Repeat it with a different last note and it sounds like a question, then an answer. Ending away from home leaves it open. Ending on the root closes it. This is the whole trick: same, same, different.", S::Roll, 2 },
        { "Steps and leaps",
          "Moving to the next note of the scale is a step. Jumping further is a leap. Steps sound smooth and singable, leaps grab attention. A good melody is mostly steps with one or two leaps. After a leap, move back the other way by step: it feels like landing.", S::Roll, 3 },
        { "Rhythm matters more than the notes",
          "Clap the rhythm of your favourite melody and you still recognise it. Play its notes with a flat rhythm and you do not. Work out the rhythm first, even on one note. Leave gaps: the rests are where the vocal, the drums and the next phrase will breathe.", S::Roll, 7 } },
      nullptr, kTabNone, nullptr },

    { "th.fit", "Melody", "Notes that work and notes that clash", {
        { "Three kinds of note",
          "Over any chord there are three kinds of note. Chord notes, the ones in the chord playing underneath, always sound right. Scale notes that are not in the chord add colour and want to move on. Notes outside the key clash. A melody is strong when its important notes are chord notes.", S::Roll, 4 },
        { "Land on chord notes",
          "The important notes are the long ones, the ones on the beat, and the last one of a phrase. Put chord notes there. Use the other scale notes in between as stepping stones. In this example every long note is in the chord under it, and it sounds settled.", S::Roll, 4 },
        { "Hear a clash",
          "Here the melody holds a long note one semitone away from a note in the chord. Two notes a semitone apart, held together, is the harshest sound in music. This is what people mean by \"that note is wrong\".", S::Roll, 5 },
        { "Fix it by one key",
          "The fix is nearly always one key away. Move the clashing note up or down to the nearest note of the chord and it resolves. When something sounds off in your beat, do not delete the idea: find the long note that rubs and nudge it.", S::Roll, 6 },
        { "Outside notes on purpose",
          "Rules describe what sounds safe, not what is allowed. A quick note outside the key that slides straight into a chord note sounds stylish, not wrong. Dark melodies use the note a semitone above home for menace. Learn where safe is first. Then step outside on purpose, briefly, and come back.", S::Keys, 5 } },
      nullptr, kTabNone, nullptr },

    { "th.shape", "Melody", "Call, response and counter melodies", {
        { "Call and response",
          "Split the melody into two halves that talk to each other. The first bar asks, the second bar answers. The answer usually copies the rhythm of the call and changes the notes, or ends lower. It is how most hooks, vocal and instrumental, are built.", S::Roll, 7 },
        { "A counter melody",
          "A counter melody is a second, quieter line that plays in the gaps of the first. When the main melody holds or rests, the counter moves. When the main melody is busy, the counter holds. If both move at once they fight, and neither is heard.", S::Roll, 8 },
        { "Octave layers",
          "The simplest layer of all: copy the melody and move the copy up twelve semitones. It thickens the line and makes it cut through on small speakers without adding a single new note, so it cannot clash.", S::Roll, 12 },
        { "Leave space for the artist",
          "A beat is half a song. If the melody fills every gap in the middle of the keyboard, where the voice lives, the rapper or singer has nowhere to go. The beats that get picked are often the ones that feel slightly empty on their own.", S::Layers, 1 } },
      nullptr, kTabNone, nullptr },
    };
}
}
