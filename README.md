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

After that the plugin updates itself. When a newer build exists an **Update ready** button appears in the header;
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
   track is, so load them while the song plays.
3. Put one on the Stereo Out. It becomes the **Master** by itself.
4. Play the loudest section. On the **Mix** tab press **Learn and balance** and keep it playing for 20 seconds.
   Every track's trim moves so the vocal leads and the mix lands at -18 LUFS with peaks under -6 dBTP. The trim is
   the track's fader: it comes after all the processing, so balancing never changes how hard a compressor is
   driven. A vocal whose loud words tower over the rest of it has its compressor switched on (the gentle two-stage
   Vocal style); if a track's peaks would still clip its channel, the whole song is turned down to make room
   instead of that track being squeezed harder.
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
- **Harmony** adds up to three voices on other notes of the key (third, fifth, octave, choir).
- **Shift** moves the whole voice up or down, as a layer or on its own, with control of the size of the voice.
- **Phone** is the voice through a small speaker. **Lo-fi** is sample-rate and bit reduction with tape wobble.
  **Motion** is chorus, flanger and phaser. **Chop** gates the sound in time with the beat.

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

The **Presets** button (whole chain) and the preset strip of every tool on the Tune and FX tabs have a **My presets**
section: Save as, Save changes, Rename, Delete, Import, Export. A star after a preset's name means a control has
moved since it was loaded. Each preset is one JSON file with the extension `.mupreset`, kept outside any song in
`~/Library/Application Support/Musically Unorthodox/Presets/<chain or tool>/`, so it is there in every project and
can be sent to someone else. The file carries a format version, stores values in real units (dB, ms, Hz), and is
written to a temporary file and swapped in, so a crash never leaves half a preset. On loading, a file from a newer
format is refused, controls this build does not have are skipped and reported, values are brought inside each
control's range, and a tool's preset can only move that tool's controls. A whole-chain preset also remembers how
loud the track was, and moves its thresholds when loaded on a quieter or louder one. The EQ, Dynamics, Tone and
Master tabs do not have per-tool presets of your own yet; a whole-chain preset covers them.

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
  what was sung, and the pulse-locked slicing is compared with the old slicing on a clean and on a rough voice.

Two lab tools run recordings through the plugin outside a host, for measuring (and listening): `MUVoiceLab` (one
tool at a time) and `MUMixLab` (a voice and a backing track through real instances, then Learn and balance).

What has not been done: none of this has been compared by ear, in a blind test, against the commercial plugins
that were given as references, and no claim is made that it matches them.

## What it does not do (yet)

- The EQ is minimum-phase and zero-latency. There is no linear-phase mode.
- Eight bands, not 24 or 32.
- Smooth works on 24 quarter-octave bands, not per FFT bin like a full spectral processor.
- No external sidechain input: instances share information through the session instead.
- Analyse on the EQ tab is a rule-based comparison against role tone curves and the other tracks. It is not a
  trained model. The AI tutor (Mix Brain tab) is separate, optional, and only explains; it never changes settings.
- Harmony is still three voices chosen by a voicing. There is no per-voice editor, MIDI or piano-roll control,
  voice leading or snapshots yet. Shift, Phone, Lo-fi, Motion and Chop are as they were in build 11.
- Pitch shifting down by an octave measures no better with the new slicing than with the old.
- Mac only (AU, VST3, Standalone). There is no Windows build.
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
