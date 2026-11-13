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
        "RULES: Answer in plain English a beginner can follow, in at most 110 words, as ordinary sentences with no markdown, lists or headings. "
        "Only recommend things this plugin can do, and always say which tab to open. Use only the numbers given in the notes and under THIS TRACK, and never invent numbers. "
        "When NOTES FOR THIS QUESTION are given, build the answer from them. If you are not sure, say what to listen for instead.\n\n"
        "WHERE THINGS ARE\n"
        "Mix tab: balances the level of every track against the lead vocal and lists collisions between tracks, each with a Fix button.\n"
        "EQ tab: every EQ move is made here. It has 8 bands, dynamic bands, and the Analyse button, which lists Mix Brain suggestions, each with an Apply button.\n"
        "Dynamics tab: compressor (styles Clean, Punch, Smooth, Glue), gate, de-esser.\n"
        "Tone tab: Smooth (tames harsh resonances), saturation, level rider, vocal space, stereo width, mono bass. There is no EQ on the Tone tab.\n"
        "Master tab: loudness target and limiter, used on the stereo output.\n"
        "Learn tab: lessons with animated examples.\n"
        "The plugin has no reverb, delay or pitch correction.\n\n"
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
        "When a Mix Brain suggestion under THIS TRACK fits the question, point to it: tell the student to open the EQ tab, press Analyse and press Apply on that suggestion.\n\n"
        "EXAMPLES OF GOOD ANSWERS\n"
        "Question: What does attack do? Answer: The compressor starts turning the sound down the moment it goes over the threshold. "
        "Attack sets how quickly it gets to the full turn-down. A fast attack clamps the start of every hit, and a slow attack lets the punch through first. "
        "Open the Dynamics tab to try it.\n"
        "Question: My guitar sounds boxy. Answer: Boxy means too much between 400 and 800 Hz. Open the EQ tab, add a bell band in that range and pull it down a few dB "
        "until it clears up. Pressing Analyse there will also list anything Mix Brain can hear.\n\n";
}

namespace
{
struct Note { const char* title; const char* keys; const char* text; };

// Mixing notes written for this plugin. Keys are the words a student might use, comma separated; a key of two
// or more words must appear as a phrase. Keep every note consistent with the lessons and the help text.
const Note kNotes[] = {
    { "Vocal gets lost in the beat", "buried,drown,drowning,lost,sit,sits,on top,cut through,cant hear,hear my vocal,vocal quiet,vocal too quiet,too quiet,vocal louder,louder vocal,behind the beat,beat too loud,louder than the vocal",
      "A vocal that gets lost needs three things, in this order. Level: on the Mix tab, Balance now sets every track against the lead vocal. "
      "Space: press Fix on any collision on the Mix tab, or switch on Vocal space on the Tone tab of the beat so it dips only while the vocal sings. "
      "Evenness: on the vocal, use the compressor on the Dynamics tab with 3 to 6 dB of reduction so quiet words stay audible. Do not just boost the vocal." },
    { "Muddy sound", "muddy,mud,muffled,unclear,cloudy,murky,thick,woolly",
      "Mud is a build-up between 200 and 400 Hz, often from several tracks stacking up there. On the EQ tab cut 2 to 4 dB with a medium-width bell around 250 to 350 Hz "
      "on the track that sounds thick, not on every track. A low cut at 80 to 100 Hz on a vocal removes rumble that adds to it. "
      "Smooth on the Tone tab with its focus set to Low mids does the same job automatically." },
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
      "Starting point for a vocal on the Dynamics tab: style Punch for rap or Smooth for singing, ratio about 4 to 1, a fast attack of 1 to 10 ms, release 40 to 80 ms. "
      "Then lower the threshold until the meter shows 3 to 6 dB of reduction on the loud words. Auto set does this from the level it has measured. "
      "If the vocal sounds flat or lifeless, raise the threshold or slow the attack." },
    { "Pumping and over-compression", "pumping,pump,pumps,breathing,squashed,crushed,lifeless,flat,over compressed,overcompressed,too much compression,ducking,sucked",
      "Pumping is the level audibly ducking and swelling back. The causes are too much reduction, a release that is too fast, or bass setting the compressor off. "
      "On the Dynamics tab raise the threshold so there is less reduction, lengthen the release, and raise the Sidechain high-pass to 80 to 150 Hz so the kick and 808 stop driving it. "
      "On the master, more than 4 to 5 dB of limiter reduction flattens the drums." },
    { "Punchier drums", "punch,punchy,punchier,hit harder,hits harder,knock,knocks,slap,slaps,snare,drums weak,weak drums,drums,transient,impact",
      "Punch lives in the first few milliseconds of a hit. On the Dynamics tab use the Punch style with a slower attack of 10 to 30 ms, so the hit gets through before the compressor clamps down, "
      "and a release fast enough to recover before the next hit. Lowering Mix blends the crushed sound with the untouched sound for weight without losing the hit. "
      "An attack that is too fast shaves the punch off." },
    { "Background noise and breaths", "noise,noisy,hiss,hum,breath,breaths,breathing noise,bleed,headphone bleed,room noise,background,gate",
      "On the Dynamics tab switch on the Gate. Set its threshold just above the level of the noise so it opens for every word and closes in the gaps. "
      "If the ends of words get chopped off, lower the threshold or lengthen the hold and release. Range sets how far it turns the track down: "
      "turning the gaps down part of the way sounds more natural than total silence." },
    { "How loud a finished song should be", "how loud,loudness,lufs,spotify,youtube,apple music,streaming,soundcloud,loud enough,quiet master,song too quiet,mix too quiet,normalise,normalize",
      "Loudness is set on the Master tab of the instance on the stereo output, with its role set to Master. Streaming master mode aims for about -14 LUFS with a true peak ceiling of -1 dB, "
      "which is the level Spotify and YouTube play at; Apple Music is about -16. Auto level sets the gain into the limiter: play the loudest section for a few seconds. "
      "Services turn louder songs down, so extra loudness only costs punch." },
    { "Mastering a song", "master,mastering,mastered,finish,finishing,final,release,upload,limiter,export,bounce",
      "Get the mix right first: on the Mix tab it should sit near -18 LUFS with peaks under -6 dB. Then on the instance on the stereo output: EQ only if the whole mix needs it, "
      "Glue compression of 1 to 2 dB at most, then the Master tab. Choose the mode, let Auto level reach the target, and check the limiter is not taking off more than 3 to 4 dB "
      "on the loudest hits. Read the Release check and listen in mono." },
    { "Clipping and levels", "clipping,clip,clips,distorting,distorted,distortion,red,too hot,peaking,peaks,headroom,gain staging,gain stage,how loud should my tracks,track levels",
      "Red on the meter means the signal went over 0 dB and clipped. Turn the track down at its source or with Output; single tracks should peak around -12 to -6 dB. "
      "Do not lean on the limiter to rescue a mix that is too hot. Balance now on the Mix tab brings the whole mix to about -18 LUFS with peaks under -6 dB, "
      "which leaves the headroom mastering needs." },
    { "Stereo width and mono", "wide,wider,width,stereo,mono,narrow,spread,panning,pan,phase,correlation,centre,center",
      "Width is on the Tone tab. Stereo width above 100 % pushes the sides out, which suits pads, backing vocals and effects. Keep the lead vocal, kick, bass and 808 in the centre. "
      "Mono bass makes everything below the chosen frequency mono. Watch Correlation: near +1 is safe, and below 0 means parts vanish in mono, so narrow the track. "
      "Always check a mix in mono, because phones and club systems are close to mono." },
    { "Matching a reference song", "reference,sound like,sounds like,match,commercial,favourite song,favorite song,industry,like the pros",
      "On the EQ tab, put the plugin on a track that is playing a finished song you like, play it and press Capture. Then on your own mix press Match: "
      "it adds a few broad bands that move your tone towards the reference, and Amount sets how far. Treat it as a guide, remove any band you do not like, "
      "and compare the two at the same loudness, because the louder one always seems better." },
    { "Where to start", "where do i start,where to start,get started,beginner,new to,first time,dont know,no idea,how do i mix,how to mix,mix a song,steps,workflow,preset,presets",
      "Start with a preset from the Presets menu at the top: complete chains sorted by what you are working on, fitted to how loud your track is. "
      "Then on the Mix tab press Learn and balance while the loudest part of the song plays. Then open the EQ tab, press Analyse and read each Mix Brain suggestion before applying it. "
      "Use A/B and Bypass to check every change really helps." },
    { "Order of processing", "order,chain,signal chain,vocal chain,what comes first,before or after,which first,sequence",
      "An order that works: clean up first (low cut, then cut mud and harsh notes on the EQ tab), then control the level (gate if needed, compressor, then de-esser after the compressor), "
      "then add tone (saturation, a little air), then width. Balance the levels on the Mix tab before reaching for any processing, because most EQ problems are really level problems." },
    { "Backing vocals and adlibs", "backing,adlib,adlibs,ad-lib,ad-libs,doubles,double,harmonies,harmony,stack,stacks,layers",
      "Backing vocals and adlibs support the lead, so they sit lower (the Mix tab puts them about 6 dB under it), thinner and wider. On the EQ tab use a higher low cut of 120 to 200 Hz "
      "and a little less 2 to 4 kHz than the lead, so they do not compete with its words. Compress them harder than the lead for an even bed. "
      "On the Tone tab widen them with Stereo width and keep the lead in the centre." },
    { "Vocals over a two-track beat", "two track,two-track,2 track,2-track,mp3 beat,stereo beat,leased beat,youtube beat,beat is one file,type beat",
      "With a two-track beat you cannot reach the instruments inside it, so make room instead. On the beat, Vocal space on the Tone tab dips the ranges the voice needs only while the vocal sings. "
      "Fix on the Mix tab, or a Mix Brain suggestion on the EQ tab, adds a small dynamic cut where the beat covers the vocal. "
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
    { "Level rider or compressor", "rider,ride,riding,uneven,inconsistent,some words,quiet words,loud words,volume jumps,automation,automate",
      "The Level rider on the Tone tab moves the volume slowly, like a hand on a fader: quiet lines come up and loud lines come down. "
      "It evens out a performance without the squashed sound. Use it for differences from one phrase to the next, and the compressor on the Dynamics tab for peaks from one word to the next. "
      "With the rider doing the slow work, the compressor needs less reduction." },
    { "Saturation", "saturation,saturate,saturator,drive,tape,tube,analog,analogue,grit,dirty,character,vibe",
      "Saturation is on the Tone tab. Tape bends both halves of the wave alike and rounds off peaks. Tube bends them unevenly and sounds fuller. Warm is the gentlest and suits bass and 808s. "
      "Drive sets how hard the sound is pushed and Mix blends it with the clean sound. Start low: on a vocal, if you can plainly hear distortion it is too much." },
    { "Comparing fairly", "better or worse,compare,comparing,a/b,ab,bypass,is it better,cant tell,louder sounds better,fooling",
      "Louder always sounds better at first, so match the loudness before judging. Auto gain on the EQ tab and Auto makeup on the compressor keep the level the same as you work. "
      "A/B at the top flips between two full sets of settings, and Bypass shows whether the plugin is helping at all. If a change only makes the track louder, it is not an improvement." },
    { "Reverb and delay", "reverb,delay,echo,room,wet,dry,ambience,hall,plate",
      "This plugin has no reverb or delay. Use the ones in your music software on a send, keep them darker and quieter than you think, and low cut the reverb at about 200 Hz "
      "so it does not add mud. Vocal space on the Tone tab is not a reverb: it makes room in the beat for the vocal." },
    { "Tuning a vocal", "autotune,auto-tune,auto tune,pitch,tune,tuning,melodyne,off key,flat notes,sharp notes",
      "This plugin does not correct pitch. Tune the vocal with the pitch correction in your music software before this plugin in the chain, so the compressor and EQ work on the tuned sound." },
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
    static const juce::StringArray tooCommon { "band", "amount", "mode", "range", "speed", "display", "listen", "show", "hear", "what", "auto", "level", "track", "tracks" };
    for (auto& [key, entry] : helpTable())
    {
        if (key == "eq.ask" || key.startsWith ("header.update") || key == "bypass") continue;
        juce::StringArray titleWords;
        titleWords.addTokens (normalise (entry.title).replaceCharacter ('/', ' '), " ()", "");
        juce::String keys;
        for (auto& w : titleWords) if (w.length() >= 3 && ! tooCommon.contains (w)) keys << w << ",";
        if (titleWords.size() > 1) keys << normalise (entry.title).trim();              // the whole name as a phrase
        if (const int s = scoreKeys (question, words, keys); s >= 2)
            controls.push_back ({ s, entry.title + " (" + tabFor (key) + ")", entry.body });
    }
    auto best = [] (const Pick& a, const Pick& b) { return a.score > b.score; };
    std::stable_sort (mixing.begin(), mixing.end(), best);
    std::stable_sort (controls.begin(), controls.end(), best);
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

juce::String notesFor (const juce::String& question, int maxMixing, int maxControls)
{
    std::vector<Pick> mixing, controls;
    choose (question, maxMixing, maxControls, mixing, controls);
    if (mixing.empty() && controls.empty()) return {};
    juce::String s ("NOTES FOR THIS QUESTION (from this plugin's own guide: build the answer from these)\n");
    for (auto& p : mixing) s << p.title << ": " << p.text << "\n";
    for (auto& p : controls) s << "The control called " << p.title << ": " << p.text << "\n";
    return s + "\n";
}
}
}
