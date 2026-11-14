#include "HelpText.h"
#include <map>

namespace mu
{
const std::map<juce::String, HelpEntry>& helpTable()
{
    static const std::map<juce::String, HelpEntry> t = {
        // ---- header
        { "header.preset",  { "Presets", "Complete starting chains, sorted by what you are working on. A preset sets every module, then fits its thresholds to how loud your track is. Load one, listen, then adjust." } },
        { "header.ab",      { "A/B compare", "Two full sets of settings. Click to flip between them and hear which is better. Shift-click copies the current settings to the other side. Trust the one that sounds better, not the one that looks better." } },
        { "header.bypass",  { "Bypass", "Switches all processing off so you can check the plugin is helping. If it only sounds louder and not better, back the settings off." } },
        { "header.settings",{ "Settings", "Track role, window size, and how this track takes part in the session balance." } },
        { "header.update",  { "Update", "A newer version of the plugin is ready. Open Settings to install it; it takes a few seconds and loads the next time you open your music software." } },
        { "eq.ask",         { "Ask the AI tutor", "Type a question about this track in your own words. The answer comes from an AI model running on this computer, and it is given this track's measurements to work from. Treat it as a second opinion and trust your ears." } },
        { "ai.model",       { "Tutor model", "The lighter model is a 2 GB download and runs on any Mac. The smarter model is 3 GB, gives fuller answers that use more of what the plugin has measured, and wants an Apple-silicon Mac with 16 GB of memory. The plugin picks the one that suits this Mac; this button switches." } },
        // ---- FX: tune, doubler, reverb
        { "tuneOn",         { "Tune", "Pitch correction for one voice. It hears the note being sung, finds the nearest note of the key and scale, and moves the voice there without changing its tone. Adds 20 ms of delay while it is on, which your music software makes up for." } },
        { "tuneKey",        { "Key", "The key of the song. Notes outside the key and scale are pulled to the nearest note inside it. If you do not know the key, press Find key after singing for a few seconds, or check the name of the beat." } },
        { "tuneScale",      { "Scale", "Which notes are allowed. Chromatic allows all twelve, so every note goes to its nearest semitone: safe when you do not know the key. Major and minor keep the voice inside the key, which is what gives hard tune its sound." } },
        { "tuneSpeed",      { "Speed", "How quickly the voice is pulled onto the note. 40 to 100 ms sounds like a singer who is simply in tune. 10 to 25 ms is tight and modern. At zero every note snaps flat onto the scale: the hard-tune effect." } },
        { "tuneAmount",     { "Amount", "How much of the correction is applied. 100 % puts the note exactly in tune. Lower values leave some of the singer's own pitch in, for a looser, more human result." } },
        { "tuneVibrato",    { "Keep vibrato", "At 100 % only the centre of each note is corrected, so vibrato, scoops and slides stay as they were sung. At 0 % every wobble is flattened as well. High for natural singing, zero for hard tune." } },
        { "tune.find",      { "Find key", "Works out the key from the notes sung so far. Switch Tune on, play a verse or a hook, then press this. It sets the key and chooses major or minor. Check it against the key of the beat when you can." } },
        { "tune.display",   { "Pitch display", "The grey line is the pitch as it was sung and the yellow line is the pitch after correction. Rows are notes; the brighter rows are in the chosen scale. A yellow line sitting on a row is a note in tune." } },
        { "dblOn",          { "Doubler", "Makes one take sound like it was recorded more than once. Each extra voice is a copy of the performance moved slightly in pitch and time, drifting the way a singer does between takes, and placed to the left or right. The lead stays untouched in the middle." } },
        { "dblMix",         { "Doubles level", "How loud the copies are against the lead. 30 to 45 % thickens a verse without being noticed. 55 % and above is an obvious stacked hook." } },
        { "dblDetune",      { "Detune", "How far the copies sit from the lead in pitch, in cents (hundredths of a semitone). 5 to 9 is tight. 12 and above is wide and lush, and starts to sound like a chorus effect." } },
        { "dblTiming",      { "Looseness", "How far the copies drift from the lead in time. Low is a tight, studio-perfect double. High is a looser, more human stack." } },
        { "dblWidth",       { "Width", "How far left and right the copies are placed. At 100 % they are hard left and right, which keeps the centre clear for the lead. Needs a stereo track: on a mono track the copies sit behind the lead instead." } },
        { "dblVoices",      { "Voices", "Double adds one copy each side. Stack adds two each side, for hooks and adlibs that should sound like a group." } },
        { "rvOn",           { "Reverb", "A clean vocal reverb. The gap before it starts and the time it takes to fade are set from the tempo of the song, so the tail breathes in time with the beat, and it steps back while the voice is singing so the words stay clear." } },
        { "rvMix",          { "Reverb level", "How much reverb sits under the dry sound. 10 to 20 % is felt more than heard, which is right for most rap and modern vocals. Go higher for hooks and ballads." } },
        { "rvAuto",         { "Timed to tempo", "On: 60,000 divided by the tempo is one beat in milliseconds. The pre-delay is a 64th note and the decay is one, two or four beats. Off: set the pre-delay and decay yourself." } },
        { "rvLength",       { "Length", "How many beats the reverb takes to fade. One beat is tight and suits fast rap. Two beats is the all-round vocal setting. Four beats is long and atmospheric, for slow songs and hooks." } },
        { "rvDecay",        { "Decay", "How long the reverb takes to fade away, in seconds. Used when Timed to tempo is off. Busy songs need short tails or the mix turns to mud." } },
        { "rvPre",          { "Pre-delay", "The gap between the dry sound and the start of the reverb. 20 to 40 ms keeps the voice up front while the reverb sits behind it. Used when Timed to tempo is off." } },
        { "rvTone",         { "Tone", "Dark reverbs sit behind the voice and sound expensive. Bright reverbs add shine but push the S sounds forward." } },
        { "rvDuck",         { "Duck", "Turns the reverb down while the voice is singing and lets it back up in the gaps. This is what keeps a wet vocal clear." } },
        { "rv.display",     { "Reverb timing", "The click on the left is the dry sound. The gap is the pre-delay. The curve is the reverb fading, drawn against the beats of the song." } },
        { "set.lowlat",     { "Low latency", "For recording through the plugin. Every instance in the session drops the stages that delay the sound: Tune reacts in about 6 ms instead of 20, and the master limiter and its loudness boost are switched off. Turn it off again before you mix." } },
        { "role",           { "Track role", "Tells the plugin what this track is, so it knows how loud it should sit and what a healthy tone looks like. Auto guesses from the sound. Put one instance on the Stereo Out and set it to Master." } },

        // ---- meters
        { "rail.peak",      { "Output level", "The peak level leaving this plugin, left and right. Keep individual tracks peaking around -12 to -6 dB so the mix has headroom. Red means it clipped." } },
        { "rail.lufs",      { "Loudness (LUFS)", "How loud the track is to the ear, averaged over the last three seconds. Peaks tell you about clipping; LUFS tells you how loud it feels." } },
        { "rail.gr",        { "Gain reduction", "How many decibels the dynamics are turning the signal down right now. On a vocal, 3 to 6 dB on the loud words is normal. More than 10 dB usually sounds squashed." } },

        // ---- mix
        { "mix.dial",       { "Mix loudness", "Integrated loudness of the whole mix measured on the Master instance, against your target. The tick is the target. Yellow means you are within 1 LU of it." } },
        { "mix.learn",      { "Learn and balance", "Play the loudest section of the song. The plugin listens to every track for 20 seconds, then sets each one's trim so the vocal leads, the beat supports it, and the mix lands on the loudness target." } },
        { "mix.balance",    { "Balance now", "Applies the moves shown on the lanes straight away, using what has been measured so far." } },
        { "mix.auto",       { "Keep balanced", "Keeps nudging the trims towards the target balance while the song plays. Useful while you are still changing sounds; switch it off before you fine-tune by ear." } },
        { "mix.lanes",      { "Track lanes", "One lane per plugin instance. The bar is how loud the track is, the tick is where it should sit for its role, and the arrow is the move that gets it there. The strip shows where its energy is, from bass on the left to treble on the right." } },
        { "mix.collisions", { "Collisions", "Places where two tracks are loud in the same frequency range and one is hiding the other. Fix adds a small dynamic cut to the less important track, only in that range and only while it is loud." } },
        { "mix.lock",       { "Lock", "A locked track keeps its level when the session is balanced. Lock anything you have already set by ear." } },
        { "trim",           { "Balance trim", "The gain the session balance has applied to this track. You can also set it by hand." } },

        // ---- EQ
        { "eq.graph",       { "EQ display", "Grey is the sound coming in, yellow is the sound going out, the white line is what the EQ is doing. Double-click to add a band, drag to move it, scroll to change its width, double-click a band to remove it." } },
        { "eqEnabled",      { "EQ on/off", "Switches the whole EQ in and out so you can check it is an improvement." } },
        { "eq.on",          { "Band on/off", "Switches this one band in and out. The quickest way to check a move is right: toggle it and listen." } },
        { "eq.type",        { "Filter shape", "Bell boosts or cuts around one frequency. Shelves raise or lower everything above or below a point. Cuts remove everything beyond a point. Notch removes one narrow frequency." } },
        { "eq.freq",        { "Frequency", "Where the band acts. Low numbers are bass, high numbers are treble. Sweep it while listening to find the problem, then decide how much to change." } },
        { "eq.gain",        { "Gain", "How much the band boosts or cuts. Cut to fix problems, boost to add character. If you are past 6 dB, the problem is probably the recording or the sound choice." } },
        { "eq.q",           { "Width (Q)", "How narrow the band is. Low Q is wide and natural, good for tone. High Q is narrow and surgical, good for removing one ringing frequency." } },
        { "eq.slope",       { "Slope", "How steeply a cut filter removes sound. 12 dB/oct is gentle and natural. 24 is the usual choice for a vocal low cut. 48 is a wall." } },
        { "eq.mode",        { "Stereo, mid or side", "Stereo affects everything. Mid affects only what is in the centre (vocal, kick, bass). Side affects only what is at the edges (wide synths, reverb). Try a side low cut to tighten the bass." } },
        { "eq.dyn",         { "Dynamic band", "The band only acts when its frequency range gets loud. Use it for problems that come and go, such as a harsh note or a boomy word, so the track is untouched the rest of the time." } },
        { "eq.thr",         { "Dynamic threshold", "How loud the band's range has to get before it starts to act. Lower it until the band moves on the problem moments only." } },
        { "eq.range",       { "Dynamic range", "The most the band will move when it is triggered. Negative cuts when loud, positive boosts when loud." } },
        { "eq.solo",        { "Listen to band", "Plays only what this band covers, so you can hear exactly what you are about to change." } },
        { "eq.delete",      { "Remove band", "Switches the band off and returns it to its default." } },
        { "eqAutoGain",     { "Auto gain", "Keeps the loudness the same as you EQ. Louder always sounds better at first, so this stops a boost from fooling you." } },
        { "eq.delta",       { "Hear the change", "Plays only what the EQ is adding or removing. If you hear music in there rather than the problem, your cut is too wide or too deep." } },
        { "eq.pre",         { "Show input", "Shows or hides the grey curve of the sound before processing." } },
        { "eq.reset",       { "Reset EQ", "Returns every band to flat." } },
        { "eq.analyse",     { "Analyse", "Compares this track's tone with what is normal for its role, checks it against the other tracks in the session, and lists what is worth fixing with the reason for each. Play the song for a few seconds first." } },
        { "eq.capture",     { "Capture reference", "Put the plugin on a track playing a finished song you like, play it, and capture. The plugin remembers its tonal balance for the whole session." } },
        { "eq.match",       { "Match reference", "Adds a few broad bands that move this track's tone towards the captured reference. The amount sets how far. It is a guide, so remove any band you do not like." } },

        // ---- dynamics
        { "comp.scope",     { "Compressor display", "The grey shape is the level coming in. The yellow at the top is how much the compressor is turning it down. The line is the threshold: drag it. Aim for the yellow to move with the loud words and drop back in the gaps." } },
        { "comp.curve",     { "Transfer curve", "Input level along the bottom, output up the side. Below the threshold the line is straight: nothing happens. Above it the line flattens: louder input gives less extra output. The dot is your signal right now." } },
        { "compEnabled",    { "Compressor", "Makes the loud parts quieter so the whole track can sit at a steadier level. It is what makes a vocal stay in front of the beat on every word." } },
        { "compStyle",      { "Style", "Clean is transparent. Punch is fast and adds bite, good for rap vocals and drums. Smooth levels gently and suits singing. Glue is slow and gentle, made for groups and the master." } },
        { "compThreshold",  { "Threshold", "The level above which the compressor starts working. Lower it until the meter shows the reduction you want: around 3 to 6 dB on the loud parts of a vocal." } },
        { "compRatio",      { "Ratio", "How firmly it holds the loud parts down. 2:1 is gentle, 4:1 is a firm vocal setting, 8:1 and above is close to limiting." } },
        { "compAttack", { "Attack", "How fast the compressor turns the level down once the sound crosses the threshold: the time it takes to do about two thirds of the job. Fast (1 to 5 ms) catches the front of every word or hit but can dull the punch. Slow (20 to 30 ms) lets the hit through first, which keeps drums snappy." } },
        { "compRelease", { "Release", "How fast the level comes back up when the sound drops again: the time to give back about two thirds of the reduction. Fast (40 to 80 ms) sounds energetic and loud. Slow (150 ms or more) sounds smooth. If you hear the level pumping, lengthen it." } },
        { "compKnee",       { "Knee", "How gradually compression starts around the threshold. A soft knee is less obvious; a hard knee is tighter." } },
        { "compMakeup",     { "Makeup gain", "Turns the track back up after compression has made it quieter." } },
        { "compAutoMakeup", { "Auto makeup", "Adds makeup gain for you so switching the compressor in does not change the level much." } },
        { "compMix",        { "Mix (parallel)", "Blends the compressed sound with the untouched sound. Crush it hard and blend a little in for weight without losing the punch." } },
        { "compScHp",       { "Sidechain high-pass", "Stops the compressor reacting to bass. Raise it to 80 to 150 Hz on a drum group or the master so the kick and 808 do not make everything pump." } },
        { "comp.auto",      { "Auto set", "Chooses a style, ratio and timings for this track's role, then places the threshold from the level it has measured. A solid starting point to adjust by ear." } },
        { "gateOn",         { "Gate", "Turns the track down when it drops below a level, removing headphone bleed, room noise and breaths between phrases." } },
        { "gateThreshold",  { "Gate threshold", "Sound quieter than this is turned down. Set it just above the noise between phrases, and check it does not cut the starts of words." } },
        { "gateRange",      { "Gate range", "How far the gate turns the quiet parts down. 10 to 20 dB sounds natural; more can sound choppy." } },
        { "gateAttack",     { "Gate attack", "How fast the gate opens when the sound starts. Keep it fast on vocals so the first consonant is not lost." } },
        { "gateRelease",    { "Gate release", "How slowly the gate closes after the sound stops. Longer keeps the tails of words natural." } },
        { "gateHold",       { "Gate hold", "How long the gate stays open after the level drops, so it does not flutter in the middle of a word." } },
        { "dsOn",           { "De-esser", "Turns down harsh S, T and SH sounds only at the moment they happen. It goes after the compressor, because compression makes them louder." } },
        { "dsFreq",         { "De-ess frequency", "Where the harshness is. Usually 5 to 8 kHz: lower for deeper voices, higher for brighter ones." } },
        { "dsThreshold",    { "De-ess threshold", "How loud an S has to be before it is turned down. Lower it until only the sharp ones trigger the meter." } },
        { "dsRange",        { "De-ess range", "The most an S is turned down. 3 to 6 dB is typical. Too much gives the singer a lisp." } },
        { "dsMode",         { "De-ess mode", "Split band turns down only the top end, which is the most natural. Wide band turns the whole signal down for a moment, which suits very bright vocals." } },

        // ---- tone
        { "smOn",           { "Smooth", "Listens across the spectrum and turns down any frequency that sticks out above its neighbours, moment by moment. It tames ringing, honk and piercing notes without dulling the whole track." } },
        { "smDepth",        { "Smooth depth", "How readily it acts and how far it cuts. 20 to 40 % is polish. Above 60 % is a rescue for a harsh recording." } },
        { "smFocus",        { "Smooth focus", "Where it looks. Full range covers everything from 200 Hz up. Low mids targets boxiness and mud. Harsh highs targets the 3 kHz and above range that tires the ear." } },
        { "smSpeed",        { "Smooth speed", "How quickly it reacts. Fast catches short harsh moments; slow is gentler and suits sustained sounds." } },
        { "sm.delta",       { "Hear what is removed", "Plays only what Smooth is taking out. It should sound thin and whistly. If it sounds like the track, the depth is too high." } },
        { "satOn",          { "Saturation", "Adds harmonics the way tape and valves do. It makes a sound thicker and easier to hear on small speakers without making it louder." } },
        { "satDrive",       { "Drive", "How hard the signal is pushed. A little adds warmth; a lot adds grit. The output is level-matched so you can judge the tone fairly." } },
        { "satMix",         { "Saturation mix", "Blends the saturated sound with the clean sound." } },
        { "satType", { "Saturation type", "Tape bends both halves of the wave alike, which adds odd harmonics and rounds off peaks. Tube bends them unevenly, which adds even harmonics too and sounds fuller. Warm is the gentlest and suits bass and 808s." } },
        { "riderOn",        { "Level rider", "Rides the volume like a hand on a fader, so quiet lines come up and loud lines come down before the compressor has to do it all." } },
        { "riderTarget",    { "Rider target", "The level the rider steers towards. Presets set this from the level of your track." } },
        { "riderRange",     { "Rider range", "The most the rider will move the level up or down." } },
        { "riderSpeed",     { "Rider speed", "How quickly the rider follows the performance." } },
        { "vsEnabled",      { "Vocal space", "For beats and instruments. While the lead vocal is singing, this dips the ranges the voice needs, then lets them back when it stops. The vocal gets room without the beat being turned down." } },
        { "vsAmount",       { "Vocal space amount", "How deep the dip is while the vocal is present." } },
        { "vsSpeed",        { "Vocal space speed", "How quickly it follows the vocal coming in and out." } },
        { "stWidth",        { "Stereo width", "Below 100 % narrows the image towards mono. Above 100 % pushes the sides out. Widen pads and backing vocals; leave lead vocal, kick and bass alone." } },
        { "stMonoBass", { "Mono bass", "Makes everything below the chosen frequency mono. Club subwoofers are usually mono and a phone is close to it, so this keeps the low end solid everywhere." } },
        { "stMonoFreq",     { "Mono below", "Frequencies under this are made mono. 100 to 150 Hz is usual." } },
        { "st.corr",        { "Correlation", "How well left and right agree. Towards +1 is safe. Around 0 is very wide. Below 0 means parts will disappear when the song is played in mono." } },
        { "outputGain",     { "Output", "Final level of this track after all processing." } },

        // ---- master
        { "masterMode",     { "Master mode", "Mix hand-off leaves headroom for a mastering engineer. Streaming master is ready to upload. Loud master is for releases that need to compete at club level." } },
        { "targetLufs", { "Loudness target", "How loud the finished track should be. Spotify and YouTube play songs at about -14 LUFS and Apple Music at about -16, so going much louder mostly costs you punch." } },
        { "targetTruePeak", { "True peak ceiling", "The highest the signal is allowed to reach, measured between the samples. -1 dBTP is safe for streaming; use -2 for very loud masters." } },
        { "masterAutoTrim", { "Auto level", "Measures the loudness of the mix and adjusts the gain into the limiter until the target is reached. Play the loudest section and give it a few seconds." } },
        { "peakGuard",      { "Limiter", "A true-peak limiter with look-ahead. It catches anything that would go over the ceiling. A few decibels of reduction on the loudest hits is normal; constant heavy reduction flattens the track." } },
        { "maxGain",        { "Limiter drive", "Pushes the mix harder into the limiter to make it louder. Watch the reduction: beyond 4 to 5 dB the drums start to lose their punch." } },
        { "master.history", { "Loudness history", "The loudness of the last minute. The line is the target. A good mix moves: verses a little under, hooks at or just over." } },
        { "master.check",   { "Release check", "What a mastering engineer would look at before signing off: loudness, peaks, how hard the limiter is working, and whether the track survives in mono." } },
        { "bypass",         { "Bypass", "Switches all processing off." } },
        { "autoBalance",    { "Take part in balancing", "When on, the session balance is allowed to move this track's trim." } },
    };
    return t;
}

HelpEntry helpFor (const juce::String& key)
{
    const auto& t = helpTable();
    auto it = t.find (key);
    if (it != t.end()) return it->second;
    return {};
}
}
