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

**Tune** is pitch correction for one voice. A YIN pitch detector hears the note every 10 ms (a leap of more than
four semitones has to be heard twice before it is believed, which keeps consonants and breaths from being mistaken
for notes). The voice is re-pitched by laying two-period slices of it closer together or further apart, which
leaves the formants (the character of the voice) where they were. Each slice is centred on a pulse of the voice
(found on a low-passed copy), the pitch handed to the engine glides between measurements instead of stepping every
10 ms, and a measurement the detector is unsure of changes nothing. Adds 20 ms of reported latency while on.

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
