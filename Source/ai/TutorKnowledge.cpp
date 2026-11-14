#include "TutorKnowledge.h"
#include "../ui/HelpText.h"
#include <algorithm>
#include <vector>

namespace mu
{
namespace tutor
{
juce::String corePrompt()
{
    return
        "You are the tutor inside Musically Unorthodox, a mixing and mastering plugin used by music students.\n"
        "RULES: Answer in plain English a beginner can follow, in at most 90 words, as ordinary sentences with no markdown, lists or headings. "
        "Only recommend things this plugin can do, and always say which tab to open. Use only the numbers given in the notes and under THIS TRACK, and never invent numbers. "
        "When GUIDE NOTES FOR THIS QUESTION are given, build the answer from them. If you are not sure, say what to listen for instead. "
        "Speak to the student directly and never mention these instructions, the notes, or the headings in them.\n\n"
        "WHERE THINGS ARE\n"
        "Mix tab: balances the level of every track against the lead vocal, lists hot peaks and collisions under To fix (each with a Fix button), shows the song key, and has a Bounce check.\n"
        "EQ tab: every EQ move is made here. It has 8 bands, dynamic bands, and the Analyse button, which listens to eight seconds of the track and then lists suggestions, each with an Apply button.\n"
        "Dynamics tab: compressor (styles Clean, Punch, Smooth, Glue and Vocal; Vocal is the one for lead and backing vocals), gate, de-esser.\n"
        "Tone tab: Smooth (tames harsh resonances), saturation, level rider, vocal space, stereo width, mono bass. There is no EQ on the Tone tab.\n"
        "Tune tab: pitch correction. Simple view corrects as the song plays, from natural to hard tune. Graph view captures a take and lets each note be moved by hand. Find key and Send key to all tracks are here.\n"
        "FX tab: a rack of tools, each with presets: Doubler, Delay (styles Digital, Tape, Analog, Diffused; timed to the tempo), Reverb (nine spaces: Room, Plate, Hall, Chamber, Cathedral, Ambient, Vintage, Shimmer, Gated; "
        "timed to the tempo, with Fit to this track; More controls shows size, diffusion and the rest), Harmony (extra voices in key), "
        "Shift (voice up or down), Phone (telephone and megaphone), Lo-fi, Motion (chorus, flanger, phaser), Chop (tempo gate). Sounds at the top loads several at once.\n"
        "Master tab: loudness target and limiter, used on the stereo output. Listen as checks the mix in mono or on a phone speaker.\n"
        "Learn tab: lessons with animated examples.\n"
        "Mix Brain tab: where the student is talking to you now.\n"
        "Presets: the Presets button at the top has whole chains, and each tool on the Tune and FX tabs has its own. Under My presets in either menu the student can save their own (Save as), and rename, delete, import or export them.\n"
        "Every track has a peak guard that stops it going over -1 dB (Settings).\n\n"
        "FACTS TO STAY CONSISTENT WITH\n"
        "An EQ turns one range of frequencies up or down and leaves the rest alone. "
        "EQ words: boomy is too much below 100 Hz; muddy is too much 200 to 400 Hz; boxy is too much 400 to 800 Hz; nasal is too much around 1 kHz; "
        "harsh is too much 2 to 5 kHz; sharp S sounds are 5 to 8 kHz; dull is too little above 8 kHz; thin is too little 100 to 250 Hz. "
        "Cut a problem before boosting anything.\n"
        "A compressor turns down whatever goes over the threshold, which evens out loud and quiet parts. Ratio sets how much it turns down. "
        "Attack is NOT a delay: the compressor begins turning down the instant the sound crosses the threshold. Attack is how quickly it reaches the full turn-down. "
        "A fast attack clamps the very start of each hit; a slow attack lets the punch of the hit through before the level has dropped. "
        "Release is how long it takes to let the level come back up afterwards. A typical vocal gets 3 to 6 dB of reduction on the loud words. "
        "On a vocal, start with a fast attack (1 to 10 ms) so every word stays level, and slow it down if the vocal starts to sound flat.\n"
        "A de-esser only ever turns down. It turns down sharp S sounds between 5 and 8 kHz and goes after the compressor. "
        "To use it, switch it on and lower its threshold until the S sounds soften. "
        "A gate turns the track down between phrases to hide background noise. "
        "Saturation adds harmonics, which makes a sound feel thicker and closer. "
        "Phone and laptop speakers cannot play much below about 150 Hz, so a deep 808 or sub bass vanishes on them. "
        "The fix for a bass or 808 that vanishes on small speakers is to switch on Saturation on the Tone tab: it adds harmonics above the note, "
        "and small speakers can play those, so the bass is heard. Filters and bass boosts do not fix it.\n"
        "LUFS measures how loud music sounds to a listener over time, not just its peaks. Streaming services play music at about -14 LUFS and simply turn louder songs down, "
        "so pushing a master louder than that gains nothing. True peak should stay under -1 dB. The Master tab sets this on the stereo output. "
        "A mix that is not mastered yet should sit near -18 LUFS with peaks under -6 dB.\n"
        "When one track covers another (for example the beat drowning the vocal), open the Mix tab and press Fix on that collision, "
        "or open the EQ tab, press Analyse and press Apply on that suggestion. Either one makes a small cut on the covering track only in the range where "
        "the two clash, so the vocal comes through without the beat getting quieter overall.\n"
        "When a suggestion under THIS TRACK fits the question, point to it: tell the student to open the EQ tab, press Analyse and press Apply on that suggestion.\n\n"
        "EXAMPLE OF A GOOD ANSWER\n"
        "Question: What does attack do? Answer: The compressor starts turning the sound down the moment it goes over the threshold. "
        "Attack sets how quickly it gets to the full turn-down. A fast attack clamps the start of every hit, and a slow attack lets the punch through first. "
        "Open the Dynamics tab to try it. "
        "Never reuse the wording of this example for a different question.\n\n";
}

namespace
{
struct Note { const char* title; const char* keys; const char* text; };

// Mixing notes written for this plugin. Keys are the words a student might use, comma separated; a key of two
// or more words must appear as a phrase. Keep every note consistent with the lessons and the help text.
const Note kNotes[] = {
    { "Vocal gets lost in the beat", "buried,drown,drowning,lost,sit,sits,on top,cut through,cant hear,hear my vocal,vocal quiet,vocal too quiet,vocal louder,louder vocal,behind the beat,beat too loud,louder than the vocal",
      "A vocal that gets lost needs three things, in this order. Level: on the Mix tab, Balance now sets every track against the lead vocal. "
      "Space: press Fix on any collision on the Mix tab, or switch on Vocal space on the Tone tab of the beat so it dips only while the vocal sings. "
      "Evenness: on the vocal, use the compressor on the Dynamics tab with 3 to 6 dB of reduction so quiet words stay audible. Do not just boost the vocal." },
    { "Muddy sound", "muddy,mud,muffled,unclear,cloudy,murky,thick,woolly",
      "Mud is a build-up between 200 and 400 Hz, often from several tracks stacking up there. On the EQ tab cut 2 to 4 dB with a medium-width bell around 250 to 350 Hz "
      "on the track that sounds thick, not on every track. A low cut at 80 to 100 Hz on a vocal removes rumble that adds to it. "
      "Smooth on the Tone tab with its focus set to Low mids does the same job automatically. A vocal that is compressed too hard also sounds thick and flat, so check that the compressor on the Dynamics tab shows no more than about 6 dB of reduction. "
      "A long reverb adds mud too: lower its level or its Low decay." },
    { "Harsh sound", "harsh,piercing,hurts,painful,shrill,tiring,fatiguing,brittle,too bright,ice pick,hi hat,hihat,hats,cymbal",
      "Harshness sits between 2 and 5 kHz. If it comes and goes with certain notes or words, use a dynamic band there on the EQ tab, or Smooth on the Tone tab "
      "with its focus set to Harsh highs, so the track is left alone the rest of the time. If it is always there, a bell cut of 2 to 3 dB around 3 kHz is enough. "
      "First check that saturation drive or a big presence boost is not what is causing it." },
    { "Sharp S sounds", "sibilance,sibilant,ess,esses,s sound,s sounds,hissy,lisp,de-esser,deesser,de esser",
      "Sharp S, T and SH sounds sit between 5 and 8 kHz. On the Dynamics tab switch on the De-esser, set its frequency where the S is loudest, "
      "and lower its threshold until the S softens but still sounds like an S. 3 to 6 dB of reduction on the S is typical. Split band mode sounds the most natural. "
      "Too much and the singer sounds like they have a lisp. It goes after the compressor." },
    { "Thin sound", "thin,weak,no body,lacks body,tinny,hollow,no weight,fuller,thicker,warmth,warmer,warm",
      "Thin means not enough between 100 and 250 Hz. On the EQ tab check the low cut is not set too high (80 to 100 Hz is plenty for most vocals) "
      "and that there is no big cut in the low mids. A gentle, wide bell of 1 to 2 dB around 150 to 200 Hz adds body. "
      "Saturation on the Tone tab, Tube type with low drive, also thickens a thin sound." },
    { "Dull sound, clarity and air", "dull,dark,clarity,clear,clearer,crisp,air,airy,sparkle,shine,brighter,bright,presence,polished,professional,expensive",
      "Clarity comes from 2 to 5 kHz, where words are understood, and air comes from above 8 kHz. On the EQ tab a wide high shelf from 8 to 10 kHz of 1 to 3 dB adds air, "
      "and a wide bell of 1 to 2 dB around 3 kHz adds presence. Remove mud first: a cut at 250 to 350 Hz often makes a track sound brighter with no boost at all. "
      "A raw vocal recording is different: it is often 6 dB or more short of a finished vocal between 2 and 8 kHz, and small boosts leave it dull. "
      "Loading a vocal preset, or Learn and balance on a vocal whose EQ has not been touched, measures that gap and fits the presence and air to it, up to 9 dB. Analyse on the EQ tab lists what is still missing. "
      "Brightening makes S sounds sharper, so check the de-esser afterwards." },
    { "Boomy low end", "boomy,boom,rumble,too much bass,bass too loud,808 too loud,bassy,low end,sub,subs,woofy",
      "Boom is too much below 100 Hz. On tracks that are not the bass, kick or 808, a low cut on the EQ tab at 80 to 120 Hz removes low end that nobody hears but that eats headroom. "
      "On the 808 itself, find the note that booms with a narrow bell, then make it a dynamic band so it only turns down when that note plays. "
      "Mono bass on the Tone tab keeps the low end solid on every system." },
    { "Kick and 808 fighting", "kick and 808,kick and bass,808 and kick,bass and kick,kick,808s,clash,clashing,fighting,low end fighting",
      "The kick and the 808 share the low end, so decide which one owns the sub. Usually the 808 holds the note below 60 Hz and the kick punches between 60 and 100 Hz. "
      "On the EQ tab dip the 808 a few dB where the kick punches, using a dynamic band so it only moves when they hit together. "
      "The Mix tab lists the clash under Collisions with a Fix button. Keep both mono below about 100 Hz with Mono bass on the Tone tab." },
    { "Bass or 808 missing on small speakers", "phone,phones,laptop,small speakers,earbuds,car,translate,translation,disappears,vanishes,cant hear the 808,cant hear the bass",
      "Phone and laptop speakers cannot play much below about 150 Hz, so a deep 808 vanishes on them however loud it is. Boosting the bass does not help. "
      "Switch on Saturation on the Tone tab of the 808 or bass: the Warm type suits them. Raise Drive until you can hear the note on a small speaker, then set Mix to taste. "
      "The harmonics it adds sit above the note, where small speakers can play them." },
    { "Compressor settings for a vocal", "compressor settings,compress my vocal,compress a vocal,compression settings,how much compression,set the compressor,set up the compressor,vocal compression,ratio,threshold",
      "Starting point for a vocal on the Dynamics tab: style Vocal, ratio about 3 to 1, attack 10 to 15 ms so the consonants get through, release about 100 ms. "
      "The Vocal style is two compressors in one: a leveller that evens out the phrases and a fast catcher that only takes the peaks, so neither has to work hard. "
      "It does not listen below 110 Hz, so a plosive or a knock does not push the next words down. "
      "Then lower the threshold until the meter shows 3 to 6 dB of reduction on the loud words. Auto set does this from the level it has measured. "
      "If the vocal sounds flat, thick or lifeless, it is being compressed too much: raise the threshold." },
    { "Pumping and over-compression", "pumping,pump,pumps,breathing,squashed,crushed,lifeless,flat,over compressed,overcompressed,too much compression,ducking,sucked",
      "Pumping is the level audibly ducking and swelling back. The causes are too much reduction, a release that is too fast, or bass setting the compressor off. "
      "On the Dynamics tab raise the threshold so there is less reduction, lengthen the release, and raise the Sidechain high-pass to 80 to 150 Hz so the kick and 808 stop driving it. "
      "On the master, more than 4 to 5 dB of limiter reduction flattens the drums, so on the Master tab lower Limiter drive or choose a quieter mode. {mixloudness}" },
    { "Punchier drums", "punch,punchy,punchier,hit harder,hits harder,knock,knocks,slap,slaps,snare,drums weak,weak drums,drums,transient,impact",
      "Punch lives in the first few milliseconds of a hit. On the Dynamics tab use the Punch style with a slower attack of 10 to 30 ms, so the hit gets through before the compressor clamps down, "
      "and a release fast enough to recover before the next hit. Lowering Mix blends the crushed sound with the untouched sound for weight without losing the hit. "
      "An attack that is too fast shaves the punch off." },
    { "Background noise and breaths", "noise,noisy,hiss,hum,breath,breaths,breathing noise,bleed,headphone bleed,room noise,background,gate",
      "On the Dynamics tab switch on the Gate. Set its threshold just above the level of the noise so it opens for every word and closes in the gaps. "
      "If the ends of words get chopped off, lower the threshold or lengthen the hold and release. Range sets how far it turns the track down: "
      "turning the gaps down part of the way sounds more natural than total silence." },
    { "How loud a finished song should be", "how loud,loudness,lufs,spotify,youtube,apple music,streaming,soundcloud,loud enough,quiet master,song too quiet,mix too quiet,normalise,normalize",
      "{mixloudness} Loudness is set on the Master tab of the instance on the stereo output, with its role set to Master. Streaming master mode aims for about -14 LUFS with a true peak ceiling of -1 dB, "
      "which is the level Spotify and YouTube play at; Apple Music is about -16. Auto level sets the gain into the limiter: play the loudest section for a few seconds. "
      "Services turn louder songs down, so extra loudness only costs punch." },
    { "Mastering a song", "master,mastering,mastered,finish,finishing,final,release,upload,limiter,export,bounce",
      "Get the mix right first: on the Mix tab it should sit near -18 LUFS with peaks under -6 dB. Then on the instance on the stereo output: EQ only if the whole mix needs it, "
      "Glue compression of 1 to 2 dB at most, then the Master tab. Choose the mode, let Auto level reach the target, and check the limiter is not taking off more than 3 to 4 dB "
      "on the loudest hits. Read the Release check and listen in mono. {mixloudness}" },
    { "Clipping and levels", "clipping,clip,clips,distorting,distorted,distortion,red,too hot,peaking,peaks,headroom,gain staging,gain stage,how loud should my tracks,track levels",
      "Red on the meter means the signal went over 0 dB and clipped. Turn the track down at its source or with Output; single tracks should peak around -12 to -6 dB. "
      "Do not lean on the limiter to rescue a mix that is too hot. Balance now on the Mix tab brings the whole mix to about -18 LUFS with peaks under -6 dB, "
      "which leaves the headroom mastering needs." },
    { "Stereo width and mono", "wide,wider,width,stereo,mono,narrow,spread,panning,pan,phase,correlation,centre,center",
      "{width} Width is on the Tone tab. Stereo width above 100 % pushes the sides out, which suits pads, backing vocals and effects. Keep the lead vocal, kick, bass and 808 in the centre. "
      "Mono bass makes everything below the chosen frequency mono. Watch Correlation: near +1 is safe, and below 0 means parts vanish in mono, so narrow the track. "
      "Always check a mix in mono, because phones and club systems are close to mono." },
    { "Is the mix balanced", "balanced,balance,balancing,levels right,right level,right levels,too loud,loud enough,quiet enough,how loud should the,turn down,turn up,which track",
      "{balance} Balance now on the Mix tab moves every track to where it should sit against the lead vocal in one press, "
      "and Learn and balance listens to the loudest section for 20 seconds first. The lanes on the Mix tab show each move before it is made. "
      "Learn and balance also fits the EQ of a vocal whose EQ has not been touched to that voice, and says so in its message." },
    { "Matching a reference song", "reference,sound like,sounds like,match,commercial,favourite song,favorite song,industry,like the pros",
      "On the EQ tab, put the plugin on a track that is playing a finished song you like, play it and press Capture. Then on your own mix press Match: "
      "it adds a few broad bands that move your tone towards the reference, and Amount sets how far. Treat it as a guide, remove any band you do not like, "
      "and compare the two at the same loudness, because the louder one always seems better." },
    { "Where to start", "where do i start,where to start,get started,beginner,new to,first time,dont know,no idea,how do i mix,how to mix,mix a song,steps,workflow,preset,presets",
      "Start with a preset from the Presets menu at the top: complete chains sorted by what you are working on, fitted to how loud your track is. "
      "Then on the Mix tab press Learn and balance while the loudest part of the song plays. Then open the EQ tab, press Analyse, let it listen, and read each suggestion before applying it. "
      "Use A/B and Bypass to check every change really helps." },
    { "Order of processing", "order,chain,signal chain,vocal chain,what comes first,before or after,which first,sequence",
      "An order that works: clean up first (low cut, then cut mud and harsh notes on the EQ tab), then control the level (gate if needed, compressor, then de-esser after the compressor), "
      "then add tone (saturation, a little air), then width. Balance the levels on the Mix tab before reaching for any processing, because most EQ problems are really level problems." },
    { "Backing vocals and adlibs", "backing,adlib,adlibs,ad-lib,ad-libs,harmonies,harmony",
      "If backing vocals or adlibs are louder than the lead, the fix is their level, not EQ: Balance now on the Mix tab puts them about 6 dB under the lead. "
      "They support the lead, so they should also be thinner and wider. On the EQ tab use a higher low cut of 120 to 200 Hz "
      "and a little less 2 to 4 kHz than the lead, so they do not compete with its words. Compress them harder than the lead for an even bed. "
      "On the Tone tab widen them with Stereo width and keep the lead in the centre." },
    { "Vocals over a two-track beat", "two track,two-track,2 track,2-track,mp3 beat,stereo beat,leased beat,youtube beat,beat is one file,type beat",
      "With a two-track beat you cannot reach the instruments inside it, so make room instead. On the beat, Vocal space on the Tone tab dips the ranges the voice needs only while the vocal sings. "
      "Fix on the Mix tab, or a suggestion from Analyse on the EQ tab, adds a small dynamic cut where the beat covers the vocal. "
      "Balance on the Mix tab first rather than boosting the vocal to fight a loud beat." },
    { "Dynamic EQ or compressor", "dynamic eq,dynamic band,multiband,eq or compressor,compressor or eq",
      "A dynamic band on the EQ tab works on one range of frequencies, and only when that range gets loud. A compressor works on the whole level of the track. "
      "Use a dynamic band for a problem that comes and goes in one range, such as a boomy note or a harsh word. Use the compressor to even out the overall loudness. "
      "A de-esser is a dynamic band made for S sounds." },
    { "How wide an EQ band should be", "q,width of,narrow,wide band,bandwidth,bell,sweep,find the frequency,which frequency,what frequency",
      "Width (Q) sets how much of the sound a band touches. Use wide bands, a Q of about 0.7 to 1.5, for shaping tone and for boosts, because they sound natural. "
      "Use narrow bands, a Q of 4 or more, to cut one ringing or boomy frequency without touching its neighbours. Boost wide, cut narrow. "
      "To find a problem, press Listen on a band on the EQ tab and sweep it until the problem jumps out." },
    { "Low cut", "low cut,high pass,high-pass,highpass,hpf,cut the lows,roll off,rolloff,filter",
      "A low cut removes everything below its frequency. On the EQ tab use one on anything that is not the bass, kick or 808: about 80 to 100 Hz on a lead vocal, "
      "120 to 200 Hz on backing vocals and bright instruments. It clears rumble and frees headroom. Slope sets how steep it is; 12 or 24 dB per octave is usual. "
      "Set it too high and the track goes thin." },
    { "Level rider or compressor", "rider,ride,riding,uneven,inconsistent,some words,in some words,quiet in some,too loud in,quiet words,loud words,every word,volume jumps,automation,automate",
      "The Level rider on the Tone tab moves the volume slowly, like a hand on a fader: quiet lines come up and loud lines come down. "
      "It evens out a performance without the squashed sound. Use it for differences from one phrase to the next, and the compressor on the Dynamics tab for peaks from one word to the next. "
      "With the rider doing the slow work, the compressor needs less reduction." },
    { "Saturation", "saturation,saturate,saturator,drive,tape,tube,analog,analogue,grit,dirty,character,vibe",
      "Saturation is on the Tone tab. Tape bends both halves of the wave alike and rounds off peaks. Tube bends them unevenly and sounds fuller. Warm is the gentlest and suits bass and 808s. "
      "Drive sets how hard the sound is pushed and Mix blends it with the clean sound. Start low: on a vocal, if you can plainly hear distortion it is too much." },
    { "Comparing fairly", "better or worse,compare,comparing,a/b,ab,bypass,is it better,cant tell,louder sounds better,fooling",
      "Louder always sounds better at first, so match the loudness before judging. Auto gain on the EQ tab and Auto makeup on the compressor keep the level the same as you work. "
      "A/B at the top flips between two full sets of settings, and Bypass shows whether the plugin is helping at all. If a change only makes the track louder, it is not an improvement." },
    { "Reverb", "reverb,room,wet,dry,ambience,hall,plate,space,tail,pre-delay,predelay,decay",
      "Reverb is on the FX tab. Fit to this track chooses the space, length, tone and ducking for this track and says why; start there. Room is small and natural, Chamber is thicker, Plate is the classic vocal reverb, Hall is large and Cathedral is huge; Ambient, Vintage, Shimmer and Gated are effects. Leave Timed to tempo on: one beat is 60,000 divided by the tempo in milliseconds, the pre-delay is a 64th note and the decay is one, two or four beats, "
      "so the tail fades in time with the song. Start with two beats and a level of 10 to 20 %. Duck turns the reverb down while the voice sings, which keeps the words clear. "
      "The plugin already keeps the reverb out of the low end; if a long reverb still sounds muddy, press More controls and turn Low decay down. Vocal space on the Tone tab is not a reverb: it makes room in the beat for the vocal." },
    { "Delay and echo", "delay,echo,slapback,throw,throws",
      "Delay is on the FX tab. With Timed to tempo on, the gap between echoes is a note length: 60,000 divided by the tempo is one beat in milliseconds, half of that is an eighth note, "
      "and a dotted eighth is three quarters of a beat, which lands between the beats and gives a line bounce. Repeats sets how many echoes follow. Keep the level at 10 to 20 % under a verse. "
      "For a throw, turn Duck up so the echo only speaks in the gaps after a line. For a slap, turn Timed to tempo off and set the time to about 90 ms with Repeats at 0. "
      "Style sets what the repeats sound like: Digital is clean, Tape is warm and each repeat softer, Analog is dark like a pedal, Diffused turns the echoes into a wash." },
    { "Harmonies", "harmony,harmonies,harmonise,harmonize,harmonizer,choir,third,fifth,backing vocals from,stack harmonies",
      "Harmony is on the FX tab. It adds voices singing the same words on other notes of the key: a third above is the usual first harmony, a fifth is open and strong, and Choir adds a low octave as well. "
      "Set the key on the Tune tab first, because the harmony notes come from that key; in the wrong key they clash. Keep the level at 20 to 45 % so they support the lead instead of competing with it." },
    { "Voice effects", "telephone,phone effect,megaphone,radio voice,deep voice,demon voice,pitched down,pitch down,chipmunk,slowed,sped up,lofi,lo-fi,bitcrush,chorus effect,flanger,phaser,stutter,chop,trance gate,robot voice,vocoder,effects on my voice,vocal effects,cool effect",
      "The FX tab has the voice effects, each with presets you can step through. Phone is the telephone or megaphone sound. Shift moves the voice up or down: an octave down at a low mix is the demon layer, a few semitones down at 100 % is the slowed-down sound. "
      "Lo-fi adds the grit and wobble of old tape. Motion is chorus, flanger and phaser. Chop cuts the voice in time with the beat. For a robot voice, load the Robot preset on the Tune tab. "
      "The Sounds list at the top of the FX tab loads whole combinations in one go." },
    { "Song key", "what key,which key,find the key,key of the song,key of my song,key of the beat,wrong key,same key",
      "The Mix tab shows the song key and where it came from. The plugin reads a key written in a track name (for example a beat called 128 bpm Gmin), or works it out by listening to the master, the beat or an instrument. "
      "Find key on the Tune tab sets it on this track, and Send key to all tracks gives it to every instance so every tuned vocal and harmony agrees." },
    { "Editing notes by hand", "graph,graph mode,note by note,one note,single note,edit notes,drag notes,move a note,fix one note,melodyne,manual tuning,capture",
      "Open the Tune tab and switch to Graph. Press Capture, play the part you want to fix, then stop the song: every note appears as a block. Press Correct all to put them on the scale, "
      "then drag any block up or down to move just that note; double-click snaps one note. Straighten flattens the wobble inside a note. The plugin has to hear the part once because it cannot see the audio file." },
    { "Peaks hitting red", "red,clipping,clips,clipped,peaking,peaks,too hot,hitting red,in the red,over 0,distorting",
      "A track that hits red while its loudness is fine has loud moments far above its average. The Mix tab lists it under To fix: press Fix and its compressor is switched on, set gently from the track's own level. "
      "Learn and balance does this by itself for vocals. If the peaks are still over after that, the answer is not to compress harder, which flattens a voice: Learn and balance turns the whole song down a little so every track has room, and the master makes the loudness back up. "
      "Every track also has a peak guard that catches anything over -1 dB." },
    { "Tuning a vocal", "autotune,auto-tune,auto tune,pitch,tune,tuning,tuned,melodyne,off key,out of tune,in tune,flat notes,sharp notes,hard tune,t-pain,robotic,key of",
      "Tune has its own tab. Set the key and scale of the song first, or press Find key; Chromatic is the safe choice when you do not know the key. The presets run from Natural vocal to Hard tune. "
      "For natural singing use a speed of 40 to 100 ms with Keep vibrato high, so only the centre of each note is corrected, and turn Keep slides up so the voice is left alone while it moves between notes. "
      "Flex leaves notes that are already close exactly as sung, and Humanise goes easier on long held notes. "
      "For the hard-tune sound set the speed to 0 and Keep vibrato to 0, and choose the major or minor scale of the song. The wrong key is what makes hard tune sound bad. "
      "If the voice warbles between two notes, turn Note hold up. To fix single notes by hand, use the Graph view." },
    { "Doubles and stacks", "double,doubles,doubler,doubling,stack,stacks,stacked,thicker vocal,wider vocal,bigger hook,layers,layered",
      "The Doubler on the FX tab makes one take sound like it was recorded more than once. Double adds a copy each side and Stack adds two each side. "
      "Keep the level around 30 to 45 % on verses so it thickens without being noticed, and 55 % or more on hooks. Detune of 5 to 9 cents is tight; more is wider and lusher. "
      "For wide doubles the plugin has to be on a stereo track, or inserted as mono to stereo on a mono one." },
    { "Frequency map", "frequency,frequencies,hz,khz,spectrum,frequency range,frequency ranges",
      "A rough map. Sub, 20 to 60 Hz: the weight of an 808. Bass, 60 to 250 Hz: kick punch and the body of bass and vocals. Low mids, 250 to 500 Hz: warmth, or mud when there is too much. "
      "Mids, 500 Hz to 2 kHz: the body of vocals and snares. Presence, 2 to 5 kHz: clarity and the attack of sounds. Highs, 5 to 10 kHz: S sounds and hi-hats. Air: above 10 kHz." },
    { "Checking a mix on other speakers", "sounds different,sounds bad in,sounds good here,other speakers,headphones,airpods,monitors,in the car,everywhere",
      "A mix has to work on speakers you are not using. Check it in mono, quietly, and on a phone. If the low end vanishes on a phone, add Saturation to the 808 on the Tone tab. "
      "If it only sounds good loud, the balance is off: use the Mix tab. Capture a finished song as a reference on the EQ tab and compare at the same loudness." },
    { "True peak", "true peak,dbtp,ceiling,inter-sample,intersample,overs",
      "True peak is the real highest level of the sound between the samples, which can be higher than the sample peaks a normal meter shows. "
      "Streaming services convert your file, and peaks too close to 0 dB distort when they do. Keep the true peak ceiling on the Master tab at -1 dB, or -2 dB for a very loud master." },
};

juce::String normalise (const juce::String& text)
{
    juce::String out (" ");
    for (auto c : text.toLowerCase())
    {
        if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '-' || c == '/') out << juce::String::charToString (c);
        else if (c == '\'') {}                       // "can't" reads as "cant", as the keys are written
        else out << " ";
    }
    return out + " ";
}

// How well a set of comma-separated keys matches a normalised question.
int scoreKeys (const juce::String& question, const juce::StringArray& words, const juce::String& keys)
{
    int score = 0;
    for (auto& rawKey : juce::StringArray::fromTokens (keys, ",", ""))
    {
        const auto key = rawKey.trim();
        if (key.isEmpty()) continue;
        if (key.containsChar (' '))
        {
            if (question.contains (" " + key + " ")) score += 3;
            continue;
        }
        for (auto& w : words)
            if (w == key || (key.length() >= 4 && w.startsWith (key)) || (w.length() >= 4 && w.endsWithChar ('s') && w.dropLastCharacters (1) == key))
            { score += 2; break; }
    }
    return score;
}

juce::String tabFor (const juce::String& key)
{
    if (key.startsWith ("mix.") || key == "trim" || key == "autoBalance") return "Mix tab";
    if (key.startsWith ("tune")) return "Tune tab";
    if (key == "trackGuard") return "Settings";
    if (key.startsWith ("dbl") || key.startsWith ("rv") || key.startsWith ("dl") || key.startsWith ("hm") || key.startsWith ("ps") || key.startsWith ("ph") || key.startsWith ("lf")
        || key.startsWith ("md") || key.startsWith ("ch") || key.startsWith ("fx.")) return "FX tab";
    if (key.startsWith ("set.")) return "Settings";
    if (key.startsWith ("eq")) return "EQ tab";
    if (key.startsWith ("comp") || key.startsWith ("gate") || key.startsWith ("ds")) return "Dynamics tab";
    if (key.startsWith ("sm") || key.startsWith ("sat") || key.startsWith ("rider") || key.startsWith ("vs") || key.startsWith ("st") || key == "outputGain") return "Tone tab";
    if (key.startsWith ("master") || key.startsWith ("target") || key == "peakGuard" || key == "maxGain") return "Master tab";
    return "top bar";
}

struct Pick { int score; juce::String title, text; };

void choose (const juce::String& rawQuestion, int maxMixing, int maxControls, std::vector<Pick>& mixing, std::vector<Pick>& controls)
{
    const auto question = normalise (rawQuestion);
    juce::StringArray words;
    words.addTokens (question, " ", "");
    words.removeEmptyStrings();

    for (auto& n : kNotes)
        if (const int s = scoreKeys (question, words, n.keys); s >= 2) mixing.push_back ({ s, n.title, n.text });

    // the plugin's own help: one entry per control, matched on the words of its name
    static const juce::StringArray tooCommon { "band", "amount", "mode", "range", "speed", "display", "listen", "show", "hear", "what", "auto", "level", "track", "tracks",
                                               "vocal", "space", "learn", "change", "with", "frequency", "input", "output", "check", "take", "part", "side", "removed" };
    for (auto& [key, entry] : helpTable())
    {
        if (key == "eq.ask" || key.startsWith ("header.update") || key == "bypass") continue;
        juce::StringArray titleWords;
        titleWords.addTokens (normalise (entry.title).replaceCharacter ('/', ' '), " ()", "");
        juce::String keys;
        for (auto& w : titleWords) if ((w.length() >= 4 || w == "eq" || w == "q") && ! tooCommon.contains (w)) keys << w << ",";
        if (titleWords.size() > 1) keys << normalise (entry.title).trim();              // the whole name as a phrase
        if (const int s = scoreKeys (question, words, keys); s >= 2)
            controls.push_back ({ s, entry.title + " (" + tabFor (key) + ")", entry.body });
    }
    auto best = [] (const Pick& a, const Pick& b) { return a.score > b.score; };
    std::stable_sort (mixing.begin(), mixing.end(), best);
    std::stable_sort (controls.begin(), controls.end(), best);
    // a weak second match pulls a small model off the point, so keep only notes that match nearly as well as the best
    while (mixing.size() > 1 && mixing.back().score * 10 < mixing.front().score * 6) mixing.pop_back();
    while (controls.size() > 1 && controls.back().score * 10 < controls.front().score * 6) controls.pop_back();
    if ((int) mixing.size() > maxMixing) mixing.resize ((size_t) maxMixing);
    if ((int) controls.size() > maxControls) controls.resize ((size_t) maxControls);
}
}

int numMixingNotes() { return (int) (sizeof (kNotes) / sizeof (kNotes[0])); }

juce::StringArray noteTitlesFor (const juce::String& question, int maxMixing, int maxControls)
{
    std::vector<Pick> mixing, controls;
    choose (question, maxMixing, maxControls, mixing, controls);
    juce::StringArray titles;
    for (auto& p : mixing) titles.add (p.title);
    for (auto& p : controls) titles.add (p.title);
    return titles;
}

juce::String notesFor (const juce::String& question, const juce::StringPairArray& facts, int maxMixing, int maxControls)
{
    std::vector<Pick> mixing, controls;
    choose (question, maxMixing, maxControls, mixing, controls);
    if (mixing.empty() && controls.empty()) return {};
    auto fill = [&facts] (juce::String text)
    {
        for (auto& key : facts.getAllKeys()) text = text.replace ("{" + key + "}", facts[key]);
        while (text.containsChar ('{') && text.fromFirstOccurrenceOf ("{", false, false).containsChar ('}'))          // a fact nobody supplied
            text = text.upToFirstOccurrenceOf ("{", false, false) + text.fromFirstOccurrenceOf ("{", false, false).fromFirstOccurrenceOf ("}", false, false);
        return text.trim();
    };
    juce::String s ("GUIDE NOTES FOR THIS QUESTION\n"
                    "This is advice written for this plugin, with measurements already worked out. Build the answer from it, in your own words. "
                    "These notes are not buttons or suggestions inside the plugin, so never tell the student to look for them.\n");
    for (auto& p : mixing) s << "- " << fill (p.text) << "\n";
    for (auto& p : controls) s << "- The control called " << p.title << ": " << p.text << "\n";
    return s + "\n";
}
}
}
