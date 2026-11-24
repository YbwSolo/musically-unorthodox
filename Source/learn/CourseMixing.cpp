#include "Course.h"

// The mixing lessons: how sound, EQ, compression, saturation, stereo and loudness work, each ending on the part
// of the plugin that does it.
namespace mu::course
{
using S = Scene;

std::vector<Lesson> mixingLessons()
{
    return {
        { "mix.sound", "Foundations", "How sound works", {
            { "A sound is a vibration",
              "Every sound is air moving back and forth. Slow vibrations are low notes and fast ones are high notes. The speed is called the frequency and is measured in hertz (Hz): 100 Hz means 100 swings every second. Each time the frequency doubles, the note goes up one octave.", S::Wave, 0 },
            { "Height is level",
              "How far the wave swings is its level, measured in decibels (dB). Decibels are a ratio, not a fixed amount: 6 dB less is half the height of the wave. Around 10 dB less is what most people hear as half as loud.", S::Wave, 1 },
            { "Real sounds are many frequencies at once",
              "A voice, a snare or an 808 is many of these waves added together. The lowest one is the note you hear. The ones above it, the harmonics, give the sound its character. The picture underneath is a spectrum: low notes on the left, high notes on the right, taller means louder.", S::Wave, 2 },
            { "The frequency map",
              "Engineers use the same few words for the same areas of the spectrum. Move the mouse across the map to read what lives in each area, what too much of it sounds like and what too little sounds like. Once you can name a problem you know where to reach.", S::FreqMap, 0 } },
          "Open the EQ", kTabEQ, nullptr },

        { "mix.eq", "EQ", "What an EQ is", {
            { "A volume knob for one part of the sound",
              "An equaliser turns one range of frequencies up or down and leaves the rest alone. Watch the middle wave: the EQ pulls it down and only that part of the sound changes. That is all an EQ ever does.", S::Wave, 3 },
            { "The bell",
              "The most used shape is the bell. It has three controls: where it acts (frequency), how much (gain) and how wide (Q). The white line is what the EQ is doing. Grey is the sound going in, yellow is the sound coming out.", S::EqShapes, 0 },
            { "Width, or Q",
              "Q is the width of the band: the centre frequency divided by how wide the band is, so a bigger number means a narrower band. A low Q is wide and gentle and changes the overall tone. A high Q is narrow and precise and removes one ringing note without touching its neighbours.", S::EqShapes, 1 },
            { "Shelves and cuts",
              "A shelf raises or lowers everything above or below a point, like a treble or bass control. A cut filter removes everything beyond a point. Its slope says how steeply: 12 dB per octave is gentle, 24 is the usual low cut on a vocal to clear rumble below 80 to 100 Hz.", S::EqShapes, 2 },
            { "Your turn: cut first, then boost",
              "This vocal has a build-up in the low mids that makes it cloudy. Boosting the highs would only make everything louder and leave the cloud in place. Drag the band onto the hump and pull it down until the yellow line is smooth. Scroll to change the width.", S::EqMove, 0 } },
          "Analyse my track", kTabEQ, nullptr },

        { "mix.room", "EQ", "Making room", {
            { "Masking",
              "When two sounds are loud in the same range, the ear follows the louder one and loses the other. This is called masking. It is why a vocal can disappear under a beat even though each sounded fine on its own.", S::Masking, 0 },
            { "Carve a pocket",
              "Turning the vocal up only starts a volume war. The better fix is a pocket: dip the beat a few decibels in the range where the words are understood, around 2 to 4 kHz, and only while the vocal is there. Vocal space on the Tone tab and the collision fixes on the Mix tab both do this.", S::Masking, 1 },
            { "Dynamic bands",
              "A dynamic band is an EQ cut that waits. It does nothing until its range gets louder than a threshold, then it dips, then it lets go again. Use it for problems that come and go, such as a boomy word or a harsh note, so the track is untouched the rest of the time.", S::EqShapes, 4 } },
          "Find collisions", kTabMix, nullptr },

        { "mix.comp", "Dynamics", "What a compressor is", {
            { "No performance is even",
              "Some words are loud and some are almost lost. Turn the whole track up so the quiet words are heard and the loud ones jump out. Turn it down for the loud ones and the quiet ones vanish.", S::Levels, 0 },
            { "Threshold",
              "A compressor is an automatic volume control. You choose a level called the threshold. Anything below it is left alone. Only the part of the sound that goes above it, shown in yellow, gets turned down.", S::Levels, 1 },
            { "Ratio",
              "The ratio sets how firmly. At 4:1, a word that goes 8 dB over the threshold comes out only 2 dB over. 2:1 is gentle, 4:1 is a firm vocal setting, and around 10:1 and above it is acting as a limiter.", S::Levels, 2 },
            { "Makeup gain",
              "The loud words are now quieter, so the whole track can be turned back up. That is makeup gain. The loud words finish where they started and the quiet words are much closer behind them. The track sounds fuller and stays in front of the beat.", S::Levels, 3 },
            { "Your turn",
              "Drag up and down to move the threshold, left and right to change the ratio. Grey is what goes in, yellow is what comes out. On a vocal, aim for the loud words to be turned down by about 3 to 6 dB.", S::Levels, 4 } },
          "Open Dynamics", kTabDynamics, nullptr },

        { "mix.attack", "Dynamics", "Attack and release", {
            { "It does not happen instantly",
              "A compressor starts turning down the moment the sound crosses the threshold, but it takes time to get there. The attack time is how long it takes to do about two thirds of the job. The release time is how long it takes to give two thirds of it back once the sound drops again.", S::Envelope, 0 },
            { "Attack shapes the hit",
              "With a fast attack the level is already down while the hit is still happening, so the crack of a drum or the front of a word is flattened. With a slower attack, 20 to 30 ms, the hit gets through before the level drops, and the sound comes out punchier than it went in.", S::Envelope, 1 },
            { "Release shapes the tail",
              "A fast release brings the level straight back up between hits, which sounds loud and energetic. Too fast and you hear the volume pumping. A slow release is smoother and suits singing and bass, but if it is too slow the level is still held down when the next hit arrives.", S::Envelope, 2 },
            { "Your turn",
              "Drag left and right for the attack, up and down for the release, and watch what happens to the front of each hit and to the gap between them.", S::Envelope, 3 } },
          "Open Dynamics", kTabDynamics, nullptr },

        { "mix.gate", "Dynamics", "Gate and de-esser", {
            { "What is between the words",
              "A microphone keeps listening when the singer stops. Headphone bleed, breaths and room noise sit between the phrases, and compression makes them louder.", S::Levels, 10 },
            { "The gate",
              "A gate is the opposite of a compressor: it turns the track down when the level falls below its threshold. Set the threshold just above the noise, and check that it is not cutting the starts and ends of words. A range of 10 to 20 dB sounds more natural than silence.", S::Levels, 11 },
            { "The de-esser",
              "S and T sounds are short bursts of energy, usually between 5 and 8 kHz. A de-esser is a compressor that only listens to that range and dips it for the moment the S lasts. It goes after the compressor, because compression makes those sounds louder. Three to six decibels is normally enough.", S::EqShapes, 5 } },
          "Open Dynamics", kTabDynamics, nullptr },

        { "mix.sat", "Tone", "Saturation", {
            { "Bending the wave",
              "Push a signal harder than tape or a valve can handle cleanly and the tops of the wave get rounded off. That bend adds new frequencies above the note that was played. They are harmonics, and they make a sound feel thicker and closer without making it much louder.", S::Saturation, 0 },
            { "Which harmonics",
              "The shape of the bend decides which harmonics appear. Bend both halves of the wave the same way and you get odd harmonics: the 3rd, the 5th, edge and bite. Bend one half more than the other, as valves tend to, and even harmonics appear too. The strongest of those are octaves of the note, so the sound gets fuller instead of edgier.", S::Saturation, 1 },
            { "Why an 808 needs it",
              "The note of an 808 sits around 40 to 60 Hz. Phone and laptop speakers play almost nothing that low, so a clean 808 simply disappears. Saturation adds harmonics at two, three and four times the note. Those are high enough to be heard, and the ear fills in the bass from them.", S::Saturation, 2 } },
          "Open Tone", kTabTone, nullptr },

        { "mix.stereo", "Tone", "Stereo and mono", {
            { "Width",
              "Stereo is two speakers. A sound that is the same in both appears in the middle. The more the two sides differ, the wider it feels. Spread pads and backing vocals out to the sides and keep the lead vocal, kick and bass in the centre, where they are solid on every system.", S::Stereo, 0 },
            { "The mono check",
              "Plenty of playback is close to mono: a phone, a club's subwoofers, a speaker across the room. In mono the two sides are added together. Anything that is opposite in the left and right cancels out and vanishes. The correlation meter warns you: towards +1 is safe, below zero is trouble.", S::Stereo, 1 } },
          "Open Tone", kTabTone, nullptr },

        { "mix.loud", "Finishing", "Loudness and mastering", {
            { "Peak is not loudness",
              "These two sounds reach exactly the same peak. One is a short hit, the other is held. The held one is far louder to the ear. Peak level tells you whether the signal will clip. Loudness, measured in LUFS, tells you how loud it feels.", S::Loudness, 0 },
            { "What streaming does to a loud master",
              "Streaming services measure every song and play them at a similar loudness: about -14 LUFS on Spotify and YouTube, -16 on Apple Music. A master pushed much louder is simply turned down. It gains nothing, and keeps the flattened peaks it paid to get there.", S::Loudness, 1 },
            { "True peak",
              "Digital audio is a row of samples, and the real wave runs between them. It can rise higher than any single sample. That is the true peak. Leave about 1 dB spare so the song does not distort when it is converted for streaming.", S::TruePeak, 0 },
            { "The limiter",
              "A limiter is a compressor with a very high ratio and a ceiling that nothing passes. It lets you raise the level until the loudest peaks touch the ceiling. A few decibels on the biggest hits is normal. Push further and every hit is shaved to the same height: louder, but flat.", S::Levels, 21 },
            { "Where to aim",
              "Before mastering, a mix should sit around -18 LUFS with peaks under -6 dB. That headroom is what the master uses. For streaming, finish at about -14 LUFS with true peak under -1 dB.", S::Loudness, 2 } },
          "Open Master", kTabMaster, nullptr },

        { "mix.chain", "Finishing", "Putting it together", {
            { "The order of a mix",
              "Work from big decisions to small ones. Balance first: most of a good mix is simply every track at the right level against the vocal. Then space: decide who wins where two tracks collide. Then tone and control on single tracks. The master comes last. It makes a good mix louder; it does not rescue a bad one.", S::Chain, 0 },
            { "A vocal chain, in order",
              "Each stage hears the result of the one before it, so the order matters. Clean up first, remove problems before the compressor reacts to them, control the level, tame the S sounds the compressor has raised, and only then add brightness and colour. The vocal presets are built in this order.", S::Chain, 1 } },
          "Load a vocal chain", kTabDynamics, "Rap vocal - punchy" }
    };
}

const char* legacyLessonId (int oldIndex)
{
    static const char* ids[] = { "mix.sound", "mix.eq", "mix.room", "mix.comp", "mix.attack", "mix.gate", "mix.sat", "mix.stereo", "mix.loud", "mix.chain" };
    return oldIndex >= 0 && oldIndex < 10 ? ids[oldIndex] : nullptr;
}
}
