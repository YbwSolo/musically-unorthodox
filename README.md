# Musically Unorthodox mix assistant

The MU studio's in-house mixing and mastering plugin. One instance goes on every track and one on the Stereo Out;
they talk to each other inside the project. Together they balance the session against the lead vocal, show where
tracks are fighting for the same frequencies, and take the finished mix to a release loudness. Every instance also
carries a full channel strip (gate, 8-band EQ, compressor, de-esser, resonance smoothing, saturation, level rider,
stereo tools) and explains each control as you use it.

Internal tool for Musically Unorthodox (In Music In Media C.I.C., Northampton). Not for public distribution.
Everything runs on the computer; nothing is sent anywhere.

## Installing and updating

Every push to `main` builds the AU, VST3 and standalone on a Mac runner, runs the tests and Apple's `auval`,
and attaches a double-click `.pkg` to a Release on the repo page. Install that `.pkg` once on each Mac and restart
Logic. If macOS says it can't be opened: right-click the .pkg > Open > Open (it is not notarised).

**Windows:** each release also carries `MusicallyUnorthodox-Windows-Setup.exe` (VST3 and a standalone app, 64-bit).
Close your music software, run it, and rescan plugins. It is not signed, so Windows asks first: More info > Run
anyway. On Windows the Update button fetches the new installer for you to run.

After that the plugin updates itself on a Mac. When a newer build exists an **Update ready** button appears in the header;
Settings > Install update downloads it, checks it against its checksum and replaces the plugin in place. It loads
the next time the host is opened. Nothing to download by hand.

How it works: each release carries `latest.json` (build number, file name, SHA-256) and
`MusicallyUnorthodox-bundles.zip`. The plugin reads `MU_UPDATE_BASE_URL/latest.json` (set in `CMakeLists.txt`),
at most once every six hours per computer. The installer hands ownership of the two plugin bundles to the person
who ran it, so no administrator password is needed to update.

**The update address has to be reachable without a login.** While this repository is private, GitHub answers the
plugin with "not found". Either make the repository public, or publish the two feed files somewhere public and
point `MU_UPDATE_BASE_URL` at it.

Logic: Audio Units > Musically Unorthodox. Also installs the VST3 and a standalone app in Applications.

## Using it

1. Put Musically Unorthodox **last** on every track, after the plugins the song was mixed with.
2. Name the tracks in your music software ("rap", "808", "beat 128 bpm Gmin"). With the role on Auto the plugin
   reads the name to know what the track is, and reads the key of the song from it when the name says it.
   Or pick a preset from the header, or set the role under Settings. Presets fit their thresholds to how loud the
   track is, and vocal presets fit their EQ to the voice that is playing (a dull recording gets more lift than a
   bright one), so load them while the song plays.
3. Put one on the Stereo Out. It becomes the **Master** by itself.
4. Play the loudest section. On the **Mix** tab press **Learn and balance** and keep it playing for 20 seconds.
   Every track's trim moves so the vocal leads and the mix lands at -18 LUFS with peaks under -6 dBTP. The trim is
   the track's fader: it comes after all the processing, so balancing never changes how hard a compressor is
   driven. A vocal whose loud words tower over the rest of it has its compressor switched on (the gentle two-stage
   Vocal style); if a track's peaks would still clip its channel, the whole song is turned down to make room
   instead of that track being squeezed harder. A vocal whose EQ has not been touched has its EQ fitted to the
   voice: presence and air sized to how far that recording is from a finished vocal, a low cut, and the de-esser
   set to watch the S sounds. The message under the button says so, and it is all on that track's EQ tab.
5. Work through **To fix** on the Mix tab: tracks hitting the top, then collisions.
6. On the master's **Master** tab choose where the song is going (mix hand-off, streaming, loud), check the mix
   with **Listen as** Mono and Phone speaker, and run the **Bounce check** on the Mix tab before exporting.

Hover over any control and the bar along the bottom says what it does, when to use it and typical settings.
The **Learn** tab has ten lessons, each a few steps long. Every step has an animated example drawn from the same
maths the plugin uses, and several end with a "your turn" exercise.

## Tune tab

**Tune** is pitch correction for one voice. A YIN pitch detector hears the note every 10 ms. The tuner waits for
the next two answers before it believes one, so it can tell a change of note from a blip (a consonant, a crack in
the voice, a wrong octave) by what came after it, and so a note that ends is known to have ended. The voice is
re-pitched by laying one-period slices of it closer together or further apart, which leaves the formants (the
character of the voice) where they were. Each slice is centred on a pulse of the voice (found on a low-passed
copy) and rises over exactly the stretch its neighbour falls over. Wherever nothing is being changed (between
words, on consonants, on a note that is already in tune) the sound is copied sample for sample, and the join
between copied and re-pitched sound is made by moving the slice, not the sound, so it leaves no dip in level.
Adds 50 ms of reported latency while on (6 ms in low latency mode, which decides on the spot as before).

**Simple** view corrects as the song plays. Key and scale choose which notes are allowed; the keyboard lets any
single note be blocked or allowed. Speed 40 to 100 ms with Keep vibrato high corrects only the centre of each note;
speed 0 with Keep vibrato 0 is the hard-tune effect. **Flex** leaves notes that are already close as they were
sung. **Keep slides** lets go of the voice while it travels between notes (measured over the last 180 ms, which is
about one cycle of vibrato) and **Humanise** corrects long held notes more gently; both fade out under 20 ms of
speed, so hard tune stays hard. **Note hold** stops a wavering voice flickering between two notes, and **Formant**
changes the size of the voice without changing the notes.

**Graph** view is for fixing a take by hand. The plugin cannot see the audio file, so it is built by listening:
press **Capture**, play the part, stop the song. The pitch is written down against the song position, split into
notes, and each note becomes a block that can be dragged to another pitch, snapped to the scale, straightened,
split or joined. During playback Tune follows the edited notes. The notes are saved with the song.

**Song key.** Find key looks first for a key written in a track name ("beat 128 bpm Gmin"), then listens to the
master, the beat or an instrument (a chord-aware key finder, not the single-voice detector), and only then to the
notes sung on the track. **Send key to all tracks** copies it to every instance in the song.

## FX tab

A rack of nine tools, each with its own switch, a live picture of what it is doing, and presets to step through.
**Sounds** at the top loads several tools at once.

- **Doubler** makes one take sound like two or four. Each extra voice is a real copy moved a few cents and a few
  milliseconds, both drifting slowly, placed left and right. The copies drop back on consonants so they do not flam.
- **Delay** is timed to the song: 60,000 / BPM is one beat in milliseconds, and the echo time is a note length of
  that (eleven lengths, from a bar to a 32nd). **Style** is what each repeat passes through: Digital (clean), Tape
  (saturation after the filters, softer top, optional wow and flutter), Analog (darker two-stage filter and uneven
  saturation, like a bucket-brigade pedal) or Diffused (all-pass filters in the loop, so repeats turn into a wash).
  **Pattern** is where they fall: Ping-pong, Straight, or Dual (right side at three quarters of the left).
- **Reverb** has nine spaces from five different engines, not one engine with nine names:
  Room, Chamber, Hall, Cathedral and Ambient are a sixteen-line feedback delay network with a lossless mixing
  matrix (so the decay time is exact), every line moving on its own slow cycle, each space with its own line
  spacing, first reflections and input diffusion; Plate is a figure-of-eight all-pass tank after Dattorro's
  published design; Vintage is an eight-line network with a band-limited input and deep movement; Shimmer feeds
  the hall's tail back an octave up; Gated is a burst of eighty reflections that holds and stops. **More controls**
  adds size, diffusion, early/late balance, low decay, movement and shimmer; **Freeze** holds the tail. With
  **Timed to tempo** on, the pre-delay is a 64th note and the decay is one, two or four beats. **Fit to this
  track** chooses the space, length, tone and ducking from what the track is, the tempo and how bright it is, and
  says why; kick, 808 and finished beats are left dry.
- **Harmony** adds up to four voices that sing the same words on other notes. Three ways to use it:
  **Voicing** picks a ready-made set; **Voices** is a table of four voices, each with its own interval (counted in
  notes of the key), level, pan, delay, size and detune; **Chords** makes those voices sing the notes of a
  sixteen-slot chord grid that is locked to the song (click notes to build each chord, or **Fill from key** for a
  common progression), each voice taking the chord note nearest to where it belongs and staying put when the
  next chord has its note too. **Voice leading** lets a voice hold its note while it still makes a third, fifth,
  sixth or octave with the tune instead of always moving in parallel. **Choir** turns every voice into three
  singers. Where the host sends MIDI to an effect (the Windows VST3 and standalone), keys held on a keyboard
  replace the grid.
- **Shift** moves the voice. **Shift** is one copy up or down, as a layer or on its own, with control of the size
  of the voice. **Wide** is two copies moved the same distance opposite ways, one each side: a few cents is the
  classic thickener. **Robot** holds everything on one note. Fine tuning in cents and a drive on the moved voice.
- **Phone** is the voice through one of five devices, each with its own band, resonances, distortion and noise:
  small speaker, telephone (300 to 3400 Hz, lopsided overdrive, line hiss), megaphone (two horn resonances, hard
  clipping), radio (an automatic gain that breathes, static, fading reception) and walkie-talkie (a burst of
  noise as each phrase ends). The noise is only there while the track is sounding.
- **Lo-fi** is a chain of small modules, each off at zero: Drive (saturation), Wobble (wow and flutter), Crush
  (lower rate and coarser steps), Wear (the level sagging and dropping out), Tone (the top rolled off) and Noise
  (vinyl ticks and rumble, or tape hiss that differs on each side).
- **Motion** sweeps something up and down: Chorus, Flanger, Phaser, Filter, Pan or Volume. What does the sweeping
  is a sine, triangle, ramp, square, or a **curve you draw** across sixteen points in the display, from sixteen
  flat steps to a smooth line. It runs freely or is **timed to the tempo** and locked to the song.
- **Chop** cuts the sound in time with the beat: an even **Pulse**, or a sixteen-step **Pattern** where every step
  has its own level, with gate length, swing and a pattern length from 2 to 16 steps.

**The order.** Doubler, Harmony and Shift work side by side on the clean voice, because they have to hear it to
follow its pitch. The other six run one after another in the order the rail shows them; drag one up or down to
change it (Reverb before Chop chops the reverb; Chop before Reverb lets the tail fill the gaps). The change is
made in a few milliseconds of silence so it cannot click. The order is one stored number, so it travels with the
song, with A/B and with your chain presets; the built-in chains and sounds use the standard order.

A mono track can load the plugin as **Mono > Stereo**, which is what lets a mono vocal have wide effects.

**Low latency** (Settings) is for recording through the plugin. Every instance in the session drops its delaying
stages: Tune reacts in about 6 ms, and the master limiter and loudness boost are off. A badge in the header shows
it is on, and it is never saved with the song.

## AI tutor (optional, free)

The **Mix Brain** tab is where questions are asked. Until a model is installed the tab shows the two on offer with
their sizes and a Download button for each; after that it is a conversation, with what the tutor can see about the
track beside it. The question goes to an AI model running on that Mac. Nothing leaves the machine, there
is no account or key, and it works without the internet. Answers come from a small model and can be wrong, which
the plugin says under every answer. Follow-up questions ("why?") are understood for ten minutes.

**Two models.** The lighter one (Llama 3.2 3B, 2 GB) runs on any Mac. The smarter one (Qwen3.5 4B, 3 GB, Apache 2.0)
gives fuller answers and makes better use of what the plugin has measured; it goes to Apple-silicon Macs with 16 GB
of memory or more. The plugin picks, and a button in Settings switches. Both are named, with their checksums, in
`scripts/make_installer.sh`, so a model can be replaced later without changing the plugin: Macs that already have
the tutor are offered the new one in Settings.

**How it is made to know mixing.** A small model knows little about mixing and nothing about this plugin, so it
answers from notes (`Source/ai/TutorKnowledge.cpp`):

- a fixed core: where everything is in the plugin and the facts the lessons teach
- the guide notes that match the question: about 35 mixing notes written for this plugin (muddy vocals, kick and 808,
  sibilance, loudness, mastering, and so on) plus the plugin's own help text for any control that is named
- measurements turned into finished sentences by the plugin (is the mix balanced and which track should move, is this
  track safe in mono, how loud the mix is against the streaming level), because small models get comparisons wrong
- this track (levels, compressor, EQ, Mix Brain suggestions) and the whole session (every track, the balance, collisions)

How it runs: the download is a helper program (the [llama.cpp](https://github.com/ggml-org/llama.cpp) server, MIT
licence, from that project's own Mac builds, pinned and checksummed in `scripts/make_tutor_pack.sh`) and one model
file from Hugging Face. They are saved in `/Users/Shared/Musically Unorthodox`, so every account on a studio Mac
shares one copy. The plugin starts the helper when someone asks; it only listens on the Mac itself and gives its
memory back when idle. Needs macOS 13.3 or newer. Settings > Remove deletes it again.

**Checked on a real Mac.** Every release build installs the tutor on the build Mac and asks both models questions
before anything is published (`MUTutorCheck`). To tune the notes or try another model, push to a branch called
`tutor-tuning`: the "Tutor answers check" workflow asks every question in `Source/tests/tutor_questions.txt` with
each model and prints the answers, without publishing anything.

If the built-in tutor is not installed but [Ollama](https://ollama.com) is running with a chat model, the plugin uses
that and no download is needed.

## Tabs

| Tab | What is on it |
| --- | --- |
| Mix | Mix loudness dial, one lane per instance (level, target, move, where its energy is), hot peaks and collisions with one-click fixes, Learn and balance, song key, bounce check |
| EQ | Input and output analysers, 8 bands (bell, shelves, cuts to 48 dB/oct, notch), per-band stereo / mid / side, dynamic bands, band solo, auto gain, delta listening, Analyse (listens to eight seconds of sound, then suggests), reference capture and match |
| Dynamics | Compressor with five styles (Clean, Punch, Smooth, Glue, and the two-stage Vocal), sidechain high-pass, parallel mix, scrolling level and reduction display, transfer curve, Auto set; gate; de-esser |
| Tone | Smooth (resonance tamer), saturation (tape, tube, warm), level rider, Vocal space, stereo width and mono bass, output |
| Tune | Pitch correction: Simple (live, natural to hard tune, flex, humanise, keep slides, note hold, formant, custom notes) and Graph (capture a take, move each note by hand), Find key, Send key to all tracks |
| FX | Doubler, Delay (four styles), Reverb (nine spaces), Harmony, Shift, Phone, Lo-fi, Motion, Chop; presets for each, your own presets, and combined Sounds |
| Master | Target modes (-18 / -14 / -9 LUFS), true-peak limiter with look-ahead, auto level, loudness history, release check, Mono and Phone speaker listening checks |
| Mix Brain | Ask the AI tutor about the track or about mixing; model download with sizes |
| Learn | Animated lessons: how sound works, what an EQ is, masking, what a compressor is, attack and release, gate and de-esser, saturation, stereo and mono, loudness and mastering, putting it together |

## Presets of your own

The **Presets** button (whole chain), the preset strip of every tool on the Tune and FX tabs, and the strip at the
top of the EQ, Dynamics, Tone and Master tabs have a **My presets** section: Save as, Save changes, Rename, Delete, Import, Export. A star after a preset's name means a control has
moved since it was loaded. Each preset is one JSON file with the extension `.mupreset`, kept outside any song in
`~/Library/Application Support/Musically Unorthodox/Presets/<chain or tool>/`, so it is there in every project and
can be sent to someone else. The file carries a format version, stores values in real units (dB, ms, Hz), and is
written to a temporary file and swapped in, so a crash never leaves half a preset. On loading, a file from a newer
format is refused, controls this build does not have are skipped and reported, values are brought inside each
control's range, and a tool's preset can only move that tool's controls. A whole-chain preset, and a preset
of the EQ, Dynamics or Tone tab, also remembers how loud the track was, and moves its thresholds when loaded on a
quieter or louder one. A tab's preset holds that tab and nothing else: never the role, the balance trim or the
output level. On Windows the presets are in `%APPDATA%\Musically Unorthodox\Presets\`.

## Sound quality: what is measured

`MUTests` asserts these on every build:

- Compressor: a 110 Hz tone and a 110 Hz + 2.5 kHz pair, 18 dB over the threshold with a 3 ms attack and 60 ms
  release, come out with harmonics and sidebands below -65 dB in every style (build 11: -42 dB in Punch, -51 dB in
  Clean). The gain is computed and smoothed in decibels, with an 8 ms hold on the peak reading so it does not ride
  each cycle of a low note.
- Reverb: every ringing space reaches the decay it is set to (mids, within 0.35 s at 2 s), its tail measures as
  smooth noise (normalised kurtosis 2.4 to 3.8, 3 being ideal), the spaces differ in when their energy arrives,
  the gated space stops, shimmer adds the octave, freeze holds, and nothing blows up at 30 s with every control
  at its limit.
- Delay: pattern timing, saturation by style and drive, top end by style, diffusion, and wobble in cents.
- Tune: hard tune lands on the note, Flex leaves close notes alone, Keep slides leaves a sliding voice nearer to
  what was sung and still tunes the note it lands on, the correction one note needed is not carried on to the
  next, and the pulse-locked slicing is compared with the old slicing on a clean and on a rough voice.
- FX tools (build 17): Phone's bands and resonances per device, noise that is only there while the track sounds,
  the walkie-talkie's burst, the radio's levelling; each Lo-fi module on its own; Motion's shapes and drawn
  curve, tempo lock, equal-power pan, the filter's range, no click from a curve that jumps, and no jump in the
  chorus as its second copy goes round (there was one, once a cycle, up to build 16); Chop's pattern, gate and
  swing, locked to the song; Harmony voices on their own intervals, levels and sides, chord notes, voice
  leading, choir; Shift's Wide (plus and minus the same cents) and Robot (one note from a sliding voice); and
  the order of the six in-line tools, all 720 of them.
- Vocal compressor: a thump before a word (the size measured on real takes) does not push the word down.
- Tone: a recording 11 dB dull from 2 kHz up gets a presence and air lift sized to the gap and inside its
  ceiling; boom under a voice does not make the analysis brighten it; bass in a beat is not treated as covering
  a voice; a suggestion applied twice adds to the band that is there and stops at its ceiling.

Two lab tools run recordings through the plugin outside a host, for measuring (and listening): `MUVoiceLab` (one
tool at a time) and `MUMixLab` (a voice and a backing track through real instances, then Learn and balance).

### Checked on real recordings (build 15)

Five sessions recorded at the studio (raw vocal and beat for each: four male rap and melodic takes on different
mics and in different rooms, one female vocal) were run through build 12 and build 15 with `MUMixLab`: a vocal
preset, Learn and balance, then Analyse with every suggestion applied. The recordings are not in this
repository. What was measured on the busiest 40 seconds of each (the five takes are in the same order in every
cell, and all three columns are measured against the build 15 curve). Builds 13 and 14 were never released: the
build machine was too slow for the tutor check on those runs. The listening pack made for the studio calls this
build 13:

| | as recorded | build 12 | build 15 |
|---|---|---|---|
| Distance of the voice's tone from the finished-vocal curve, 125 Hz to 8 kHz (dB) | 4.7, 2.7, 6.3, 6.8, 3.8 | 3.5, 2.9, 4.0, 4.9, 2.5 | 1.9, 2.9, 2.5, 4.2, 1.6 |
| Low-mid weight: 200 to 500 Hz against 1 to 4 kHz (dB) | +6.9, +2.9, +3.6, -0.3, +9.0 | +6.8, +2.4, +5.1, +2.6, +7.4 | +5.7, +2.5, +2.9, +0.8, +6.0 |
| How uneven the words are: spread of syllable levels (dB) | 3.4, 4.0, 5.8, 5.2, 4.4 | 2.4, 3.1, 3.8, 2.7, 3.8 | 2.1, 2.9, 3.6, 2.8, 3.5 |

Build 12 left two of the five voices heavier in the low mids, against their presence, than they were recorded
(+1.5 and +2.9 dB), which is the muddy result that was reported. What was found and changed:

- The raw voices sit about where the published long-term spectrum of speech sits or below it: 6 to 17 dB under
  the old target at their dullest point between 2 and 8 kHz. Build 12 answered with 2 to 4 dB. The lift is now
  sized to each recording.
- The old target itself asked for more presence than a mix adds (about 10 dB over a natural voice at 2 to 3
  kHz). It is now the speech spectrum plus a normal mixing lift of 5 to 7 dB.
- The "thin" rule looked at 120 to 250 Hz, where a woman's voice or a high male voice has nothing, and asked
  for 3 to 6 dB at 180 Hz. It now looks at 180 to 380 Hz, and the automatic fit never adds weight under 500 Hz.
- Collisions were being reported at 50 and 130 Hz between the beat and the voice. A voice is now only defended
  between 250 Hz and 6 kHz.
- A suggestion could replace a band that was doing another job when no band was free. It now joins a band of
  its own kind or is left out, and nothing automatic takes the EQ past about 10 dB in one place.
- On two takes a plosive or knock was the loudest thing in the recording, and the Vocal compressor used without
  its sidechain filter held the next words down (the words came out less even than they went in). The Vocal
  style now never listens below 110 Hz, and its leveller lets go a little sooner.
- Gentle tuning left most real notes where they were sung: the centre of a note was a slow average that started
  from the scoop into it, the correction for one note carried into the next, and Keep slides held the correction
  off for a fifth of a second after the voice had landed. Held notes whose centre ends within 15 cents of the
  key, natural settings, three sung takes: as sung 33 / 32 / 95 %, build 12 46 / 30 / 56 % (it made the third
  take worse), build 15 55 / 42 / 90 %. Hard tune: 99 / 100 / 100 %.

The finished-vocal curve is reasoned from published speech spectra and normal mixing practice. It was not
measured from finished vocal stems, and it is the thing most worth checking by ear.

What has not been done: none of this has been compared by ear, in a blind test, against the commercial plugins
that were given as references, and no claim is made that it matches them. The measurements above say the voice
is brighter, lighter in the low mids and more even than in build 12; they do not say it sounds good.

### EQ analysis (build 19)

Above the Analyse list on the EQ tab you now say what the track is. **Track kind** uses the kind set for the whole
plugin. **Vocal** and **Instrument** name it exactly: lead, rap, high or low voice, backing vocals, adlibs; kick,
snare, hi-hats, drum bus, 808, bass, piano, acoustic and electric guitar, synth lead, pad, brass, a full beat,
sound FX. Each has its own tone curve and its own list of what goes wrong where (a snare is boxy at 350 to
700 Hz, a hi-hat is clangy at 800 Hz to 2 kHz, a piano has hard hammers at 2 to 4.5 kHz). The curves for the
named instruments are reasoned from how the instruments are built and from ordinary mixing practice. They were
not measured from a library of stems.

Analyse does three jobs, in the order an engineer does them, and each can be switched off:

- **Surgical**: what should not be there. A low cut under the source, fixed cuts for mud, boxiness and
  honk, and a narrow cut for a single ringing note (a band standing 5 dB or more clear of the bands either
  side). The analysis hears in thirds of an octave, so a ringing note is placed to within that; it is not
  looked for on a bass, a kick or a whole beat, where one band standing out is the note or the tune.
- **Tonal**: what is good and wants bringing out. Broad bells and shelves, worked out on the spectrum as it will
  be once the surgical cuts are in, so a voice is not given presence to make up for mud that is about to go.
- **Dynamic**: what is only there some of the time. A build-up that came and went during the eight seconds of
  listening gets a cut that only works while it is there, sibilance goes to the de-esser, and a clash with
  another track gets a dynamic cut on the track that should give way.

Build 21 added **Main things / Detailed**. Detailed (the default) multiplies every limit by 0.6, so smaller
differences from the usual tone are listed too, and allows four suggestions per job and twelve in all.

Build 21 also separates **notes from resonances**. On a sung vocal the tall narrow peaks on the display are
the notes and their harmonics, and they look like the obvious thing to cut. The analyser now finds the notes
that were held while it listened (narrow lines between 70 and 1500 Hz), and anything that is one of them or a
whole multiple of one is never suggested as a cut; the summary says which peaks are the notes. This also
corrected build 19 and 20: checked against the whole recordings, the "Ringing around 570 Hz" and "Ringing
around 460 Hz" they reported on two of the five test vocals were the note sung most in each song (D5 in a
song in G, A4 in a song in A minor), not resonances. A narrow peak that is not a note is listed as a likely
resonance with its exact frequency (up to three, on voices, melodic instruments and drums), worded as
something to solo and sweep before cutting. Limits: high up, the harmonics of several notes lie closer
together than they can be told apart, so on a sung vocal almost nothing above about 3 kHz is offered; and a
resonance that happens to sit on a note of the song is missed. Two narrow cuts close together are no longer
merged into one band. On the five raw test vocals Detailed lists 2 to 4 things of its own each, plus the beat
covering the vocal; one narrow peak was offered across the five (5.7 kHz on one song), and whether it is a
resonance has not been confirmed by ear.

**A beat with no stems.** Vocal space (Tone tab) dips a beat only while the lead vocal is singing. Its three dips
used to be fixed at 260 Hz, 2.6 kHz and 7 kHz. **Fit to the vocal** compares what the beat and the lead vocal
have been measured doing and puts up to three dips where the beat is within 6 dB of the vocal in a range the
vocal is strong in, sized 1.5 to 6 dB. **Centre only** dips the middle of the stereo picture and leaves the
sides. On the five studio sessions, after Learn and balance, it found one place on each beat, between 190 and
300 Hz, 2 to 3 dB deep: these beats were already clear of the vocal in the presence range. Analyse on a beat
offers the same thing as "Make room for the vocal". It follows the level of the vocal, not what the vocal is
singing, and nobody has judged it by ear yet.

### Mastering assistant (build 20)

On the instance that sits on the Stereo Out, the Master tab opens on **Assistant**. Press **Analyse & master**,
play the finished song from start to end, and press it again (it needs at least 20 seconds of sound). A second
view, **Loudness and limiter**, is the tab as it was.

**What it measures**, on the mix as it arrives: integrated, short-term and momentary loudness (ITU-R BS.1770),
true peak, loudness range (EBU Tech 3342), crest, how far the hits stand above the level around them, the
long-term spectrum in 32 bands and how each band comes and goes over the whole song, how much louder the loud
bass notes are than the usual bass level (40 to 120 Hz), stereo correlation and its lowest value, width in four
ranges, how much is lost in mono, and whether the mix arrives already clipped.

**What it decides.** A set of rules worked from those measurements chooses which processors the song needs. It is
not a trained model, nothing is random, and the same measurements always give the same answer.

| Processor | Used when |
|---|---|
| Corrective EQ | a range is over the tone being aimed for by about 1 dB or more (build 21: fitted, see below); rumble under 30 Hz |
| Tonal EQ | a range is under the tone being aimed for by about 1 dB or more; lifts are kept to 70 % of what cuts are allowed |
| Dynamic EQ | harsh upper mids (always dynamic on a master), or a low-mid build-up that comes and goes |
| Multiband compression | loud bass notes 3 dB or more above the usual bass level; low band only, ending where this song's bass ends |
| Glue compression | the style asks for it or the song moves over more than 9 dB, and the mix is not already dense |
| Saturation | the Warm, Loud and Aggressive styles, unless the mix is dense or clipped |
| Mono bass, width | the low end is wide; a phase problem narrows the mix and is sent back to the mixer; a narrow, in-phase mix is widened a little in the loud styles |
| Soft clipper | the loud styles, when the hits are tall enough to be worth rounding |
| True peak limiter | always: ceiling -1 dBTP; level from the whole-song measurement (build 21, see below) |

From build 21 the **level** is set from the measurement of the whole song and then left alone. Before, the
level kept being stepped every 2.5 s towards the target from whatever passage was playing, so a quiet verse
pushed it up and the chorus came out over the target (1.2 LU over on the most dynamic of the five test mixes),
and it was not saved: a project opened again, or bounced straight away, started 0 dB and found its level over
the first seconds of the song. Now the starting gain is target minus the song's loudness; the mix going in
and the master coming out are then measured over the same 12 s, the gain is corrected once by the difference
(what the EQ, compression and limiter added or took away), and the result is saved with the project. Changing
the loudness target by hand hands the level back to Auto level. On the five mixes, Transparent and Loud, every
master is now within 0.5 LU of its target at -1 dBTP.

From build 21 the master EQ is **fitted to the target** instead of only reacting to large faults: up to five
broad bands (shelves at the ends) take out a share of the difference between the mix and the tone being aimed
for, 30 % at the lowest intensity, 60 % in the middle and 90 % at the top, wherever the difference is over about
1 dB. Cuts go to 3.6 dB at most in the middle of the intensity range and lifts to 2.5 dB; a shelf is used once
per end, and a lift is never placed right beside a cut. The first version only moved for differences of 2.5 to
4 dB and by about half, which on a decent mix meant a limiter and little else: on the five test mixes it made
one or two EQ moves of 0.9 to 1.8 dB, and now makes five or six of up to 3.6 dB. The difference is measured from the middle value of the mix, so one heavy
range does not get every other range boosted. This is the same idea the documentation of the best-known
mastering assistant describes (a target curve, then shelves and bells set to match it); what that product has
and this does not is target curves measured from a large library of released music.

Six reasoned **styles** (Transparent, Warm, Loud, Dynamic, Bright, Aggressive; build 22 adds the measured
House style, below) lean the tone aimed for, the loudness and
which of the optional processors are considered. **Intensity** scales every move (EQ gains, compression,
saturation, clipping) from 0.4 to 1.3 times; the loudness target moves by under 1 dB over its range. Loudness is
**held back** when reaching the style's target would take more off the peaks than the style allows (3 dB for
Transparent, 7 for Aggressive), and the report says by how much. A **reference** (a file, or a track played
through the plugin) replaces the style's tone and loudness, with no band of the target moved more than 4 dB.

Two new processors were written for it: a three-band compressor (fourth-order Linkwitz-Riley crossovers; with no
compression its bands add back up to the input within 0.1 dB at every frequency, crossovers included; out of the
path entirely when off) and a soft clipper in front of the limiter (four times oversampled; an exact delay when
off). The EQ, glue compressor, saturator, stereo tool and limiter are the ones already in the plugin.

After it has set everything, it measures the master over 30 seconds and writes a **report**: each line is the
figure before and after, and a verdict ("Improved", "Steadier") appears only where a processor was used and the
measurement moved. **Listen to** switches between the master, the master turned down to the mix's loudness, and
the mix as it arrived, for a fair comparison (never in a bounce). **Decide again** re-plans from the same
measurements with a new style or intensity; **Undo** puts everything back.

On the five studio sessions (the mixes made by the plugin's own Learn and balance, 30 seconds each), every
master landed within 1 LU of its target and at -1.0 dBTP. The chains differed from song to song: Transparent gave
one shelf and a limiter on one song, a low cut, a high-shelf cut and glue on another (9.4 LU of loudness
range), and a cut, a presence lift and low-band compression on a third (bass notes 6.6 dB over the usual
level, 3.8 dB after). Loud asked for -9.5 LUFS and was held back to between -9.7 and -10.6 on three of the five.

What it does not do yet: linear-phase EQ, mid/side decisions (the EQ has mid/side bands, the assistant does not
use them), mid and high band controls on screen (they are host parameters), before and after waveforms,
multi-step undo, a frequency-dependent exciter, and reordering of the processors. It cannot repair one track
inside a mix; when it measures a problem that belongs in the mix (clipping, phase, a mono mix, a thin low
end) it says so. The targets are reasoned, not measured from released masters. Nobody has judged a master
from it by ear yet, and no claim is made that it matches the products that were given as references.

### House: a target measured from finished records (build 22)

Until build 22 every target in the mastering assistant was reasoned from mixing practice. The studio then chose
five finished commercial releases (rap, drill and R&B) as its standard, and they were measured over the whole
song with the plugin's own analyser. **House (measured)** is a seventh style, and the default, that aims at
what they have in common. The audio itself is not in this repository; only the averages below are.

What was measured, and what it showed about the reasoned targets:

| | The five records | Before build 22 |
|---|---|---|
| Tone, 34 to 66 Hz (against the average of 60 Hz to 9 kHz) | +10 to +13 dB, within 1 to 2 dB of each other | the reasoned target sat 4 to 10 dB lower, so a fitted EQ cut the sub of a rap mix |
| Tone, 1.7 to 5 kHz | 4 to 7 dB under that average | reasoned target 0.6 to 2.6 dB higher |
| Tone, 6 to 12 kHz | 5 to 9 dB under | reasoned target 2.4 to 3.8 dB lower (duller) |
| Integrated loudness | -10.4, -10.2, -10.0, -9.1, -8.0 LUFS | House aims at -10.0 |
| Loud bass notes above the usual bass level | 1.6 to 4.9 dB | called "uneven bass" from 3 dB; now from 5.5 |
| Lowest stereo correlation | +0.3 down to -0.7, with under 0.5 dB lost in mono | below -0.3 was called a phase fault and the master narrowed; now it also has to cost 1.5 dB in mono |
| Sides against the middle, whole range | 9 to 17 dB under | under 14 dB was called narrow; now under 18 |
| Sides against the middle, 800 Hz to 5 kHz | 7 to 10 dB under | not looked at |
| Sound below 30 Hz | 5 to 15 dB under the sub above it | could be read as rumble and cut; now it has to be within 3 dB of the sub |

How House uses it. The tone target is the average of the five, and each band has a **tolerance**: how much the
five differ among themselves there (1 to 2.5 dB, never under 1). A mix inside the tolerance is as close to the
target as the records are to each other and is left alone; beyond it, only the part beyond it is corrected,
inside the same limits as every style (3.6 dB of cut and 2.5 dB of lift in the middle of the intensity range).
The reasoned styles got a tolerance too, in the bottom octave and a half only (4 dB under 70 Hz), because how
much sub a song carries is not something a reasoned shape can judge. A mix that arrives within 2 LU of the
loudness aimed for is treated as **already mastered**: EQ and ceiling only.

Checks. The five records themselves, put through House: two get no EQ, three get one or two moves of 0.7 to
1.3 dB, none gets compression, saturation, clipping or widening, and their loudness comes out at -10.0 to -10.4
LUFS. The five studio test mixes, through House at intensity 50: tone distance from the records' average goes
from 1.9, 3.9, 2.6, 2.9, 1.9 dB to 1.6, 2.6, 2.1, 2.3, 1.7 dB (the records themselves are 1.2 to 2.1 dB from
their own average), and all land within 0.25 LU of their loudness target, measured independently. They are
denser than the records: peaks 9.1 to 11.5 dB above the average level against 11.3 to 12.6, part of which is
the -1 dBTP ceiling (the records peak at full scale) and the lossy copies' overshoot.

What House cannot do, and says in its notes with the figures. Three of the five test mixes are 7 to 12 dB
lighter than the records between 34 and 66 Hz. Between 800 Hz and 5 kHz four are 6 to 15 dB narrower than
the middle one of the records, and the fifth is mono. A master lifts the low end 2 to 3 dB and widens a few per cent; the rest is the 808 level and the
panning, doubles and stereo effects in the mix. Those two differences, not the mastering chain, are most of
the measured distance between these mixes and the records.

Limits of the measurement. Five songs of one family of genres: House is that sound, not a general target. The
copies measured were lossy and cut off at 15.8 kHz, so nothing above 12 kHz is taken from them (the two top
bands continue the slope below) and their true peaks say nothing. The 30 s excerpts of the test mixes may not
be the loudest part of each song. Nobody has compared a House master with the records by ear yet.

The **level** measurement changed again in this build: the gain of the master is now measured only while the
mix is within 10 LU of the whole song's loudness, because measured on a quiet intro the master looks louder
than it will be once the compressors and the limiter are working (a finished record measured the build 21 way
came out 2.4 LU under its target for that reason). It is corrected twice, after 12 s and after the 30 s check.
Loading a reference **file** was tested on real FLAC and WAV files at 44.1 kHz for the first time (same
figures as measuring the samples directly); MP3 and M4A still depend on the computer and are untested.

### Checked on real recordings (build 18): Tune

Tune was reported as glitchy on real vocals. Three sung takes from the same sessions (a woman, two men, one of
them low and raspy) were tuned by build 17 and build 18 and compared with the recordings, 10 ms at a time. Two
things were counted: moments where the recording is a clean note and the tuned voice is not ("rough"), and
moments where the recording has a clear note and the tuned voice has none ("lost"). Per minute of singing, the
three takes in the same order:

| | build 17 | build 18 |
|---|---|---|
| Rough, speed 20 | 111, 71, 404 | 9, 2, 20 |
| Rough, natural preset | 73, 41, 283 | 4, 0, 0 |
| Rough, hard tune | 225, 90, 617 | 65, 14, 184 |
| Lost, speed 20 | 51, 37, 174 | 2, 0, 5 |
| Lost, hard tune | 92, 38, 283 | 13, 4, 65 |
| Dips of 3 dB or more in level, hard tune | 0, 0, 20 | 0, 0, 0 |

What is left in hard tune is the snap from one note to the next, which is the effect. Tuning accuracy on the
same held notes is unchanged within one note either way (hard tune 99 / 97 / 96 % of held notes centred within
15 cents, speed 20 98 / 100 / 93 %). What was found:

- A voice with a strong second harmonic (the low, raspy take) was read an octave too high for 20 to 30 ms, 25
  times a minute, and the tuner followed it there and back. The detector now checks the dip at twice the delay.
- Every big interval began with 10 to 30 ms of the new note cut at the old note's size, because a leap was only
  believed on its second hearing and the voice was not played late enough for that. The tuner now looks 20 ms
  ahead and plays the voice 50 ms behind.
- Going from untouched sound to re-pitched sound (every word, and in gentle settings many times a second) took
  the sound from up to half a period away, which laid the same wave twice out of step for a few milliseconds.
- Slices had one width for both sides, so a pitch that dropped left a gap between two slices and one that
  jumped up piled them on top of each other.
- After a change of note the slices could sit between two pulses of the new note until they happened to line up.

These are measurements, made with a second pitch tracker that is not perfect either (on the raspy take it
reads the recording and the tuned voice an octave apart 100 to 200 times a minute, in both builds). Nobody has compared build 18 by ear yet. Harmony and Shift use the same slices and measure a third to a
half less rough than before, but they do not look ahead yet. The natural preset still centres only about half
of the notes that were sung off (49 / 92 / 37 %): it is smooth, and it is gentle to a fault.

## What it does not do (yet)

- The EQ is minimum-phase and zero-latency. There is no linear-phase mode.
- Eight bands, not 24 or 32.
- Smooth works on 24 quarter-octave bands, not per FFT bin like a full spectral processor.
- No external sidechain input: instances share information through the session instead.
- Analyse on the EQ tab is a rule-based comparison against role tone curves and the other tracks. It is not a
  trained model. The AI tutor (Mix Brain tab) is separate, optional, and only explains; it never changes settings.
- MIDI control of Harmony does not work in Logic. Logic never sends MIDI to an Audio Unit effect; that takes a
  second, "MIDI-controlled effect" version of the plugin, which has not been built. On a Mac the chords come
  from the grid. The MIDI path exists and is tested, and is switched on in the Windows VST3 and standalone.
- Harmony's chord grid holds pitch classes (which notes), not octaves or timing inside a slot: it is a chord
  track, not a full piano roll with note lengths. There are no harmony snapshots.
- The Motion curve has sixteen fixed points, not free breakpoints, and one curve, not one per target.
- None of the rebuilt FX tools has been heard by a person yet. Their tests measure what they do, not how they
  sound.
- Pitch shifting down by an octave measures no better with the new slicing than with the old.
- Windows is new and has not been run in a real Windows music program yet. It builds, its sound tests pass
  on a Windows machine and the installer is made, but nobody has opened it in a host. On Windows there is no
  Audio Unit (that is a Mac format), an update is fetched as a new installer instead of installing itself, the
  built-in AI tutor is not included (Mix Brain works there only through Ollama), and the installer is not
  signed, so Windows asks before running it.
- It cannot change the settings of other plugins; it sits after them and corrects the result.
- Tune works on one voice at a time, not on chords or a full beat. The Graph view moves the pitch of notes; it
  does not move them in time, and it has to hear a part once before it can show it (there is no ARA link to the
  audio file).
- Every track has a peak guard at about -1 dB (Settings). It is a safety net, not a mix limiter.

## Building on a Mac by hand

`xcode-select --install`, `brew install cmake`, then `bash scripts/build_mac.sh`. Fetches JUCE 8 the first time,
builds universal binaries, runs tests + auval, installs to `~/Library/Audio/Plug-Ins`.
`bash scripts/make_installer.sh` packages a `.pkg` from a build.

## Project layout

```
CMakeLists.txt                   JUCE 8 project (AU, VST3, Standalone, MUTests, MUSession, MUPreview)
Assets/                          logo, icons, Anton / Inter fonts baked into the binary
scripts/build_mac.sh             one-shot Mac build + auval
scripts/make_installer.sh        .pkg installer
.github/workflows/build-mac.yml  CI: Mac build -> tests -> auval -> pkg -> Release
Source/
  PluginProcessor.*              chain, parameters, actions (presets, Mix Brain, reference, A/B), state
  PluginEditor.*                 window, 60 fps tick, page transitions
  state/ParameterIDs.h           every parameter id (never rename after release)
  state/ParameterLayout.*        ranges, defaults, display
  state/ChainPresets.*           preset library
  dsp/LoudnessMeter.*            BS.1770 LUFS + true peak
  dsp/TruePeak.h                 4x inter-sample peak detector
  dsp/SpectrumAnalyzer.*         4096-point FFT, 75 % overlap, long-term bands
  dsp/EQEngine.*                 8-band EQ: slopes, mid/side, dynamic bands, auto gain, solo
  dsp/Compressor.*               four detector/release behaviours, sidechain high-pass, history
  dsp/Smoother.*                 resonance tamer
  dsp/Maximizer.*                true-peak look-ahead limiter (master instance only)
  dsp/StereoTool.*               width, mono bass, correlation
  dsp/Gate.* DeEsser.* Saturator.* Rider.* VocalSpace.* RoleDetector.*
  dsp/Pitch.*                    YIN pitch detector; grain engine (pitch and formant shifting)
  dsp/Tuner.* TuneGraph.*        pitch correction: live, and following hand-edited notes captured against the timeline
  dsp/KeyFinder.*                key of a full mix from a chromagram; keys written in track names
  dsp/Doubler.* PitchFX.*        doubles; harmony and shift voices
  dsp/Echo.* Reverb.*            tempo-synced delay; FDN reverb (room, plate, hall)
  dsp/ColourFX.*                 phone, lo-fi, motion (chorus, flanger, phaser), chop
  dsp/PeakGuard.h                zero-latency safety limiter on every track
  analysis/ReverbAdvisor.*       chooses a reverb for a track from its role, the tempo and its tone
  state/ModulePresets.*          presets for each tool, and the combined Sounds
  analysis/TonalAnalysis.*       role tone curves, suggestions, clash wording, EQ match
  session/SessionBus.*           process-wide bus shared by all instances, learn and balance, shared reference
  session/BalanceEngine.*        role targets, balance plan, masking detection
  update/Updater.*               in-plugin updates: feed check, download, checksum, in-place install
  ai/LocalAI.*                   optional AI tutor: built-in helper and model, or a local Ollama server
  ai/TutorKnowledge.*            what the tutor is told: core facts, mixing notes, lookup by question
  state/LocalSettings.h          per-computer settings (update checks, lesson progress)
  ui/                            theme, icons, look and feel, widgets, help text, pages
  tests/TestMain.cpp             DSP and analysis tests
  tests/SessionHarness.cpp       4-instance offline session test
  tests/PreviewHarness.cpp       live UI preview fed with synthetic stems
  tests/mock_ollama.py           stand-in AI servers for the harness (also mock_tutor_helper.py)
  tests/TutorCheck.cpp           runs the real tutor end to end on the build Mac
```

## Notes

- Signal chain per instance: trim > gate > EQ > compressor > de-esser > smooth > saturation > rider > vocal space >
  stereo > output > (master only) auto level > limiter.
- Latency: about 1 ms on track instances (saturation oversampling), so vocals can be recorded through it. The
  master adds 4 ms of limiter look-ahead. Both are reported to the host.
- The session is balanced to a mix level with headroom (-18 LUFS, -6 dBTP). The master stage then takes that mix to
  the final target, so mastering gain never feeds back into the track balance.
- All instances must be in the same host process to see each other (normal for Logic, Ableton, FL, Reaper, Cubase).
- Track names come from hosts that provide them (VST3 hosts do; Logic AU does not), otherwise role + number.
- Brand: black / white / MU yellow #FBF20D, Anton for headlines, Inter for text (both OFL, bundled).
- Icons in `Assets/icons` were supplied for this project. Check their licence before the plugin is distributed
  outside the studio.
