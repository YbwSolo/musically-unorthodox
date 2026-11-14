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
   Every track's trim moves so the vocal leads and the mix lands at -18 LUFS with peaks under -6 dBTP. A vocal
   whose loud words tower over the rest of it has its compressor switched on as well.
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
leaves the formants (the character of the voice) where they were. Adds 20 ms of reported latency while on.

**Simple** view corrects as the song plays. Key and scale choose which notes are allowed; the keyboard lets any
single note be blocked or allowed. Speed 40 to 100 ms with Keep vibrato high corrects only the centre of each note;
speed 0 with Keep vibrato 0 is the hard-tune effect. **Note hold** stops a wavering voice flickering between two
notes, and **Formant** changes the size of the voice without changing the notes.

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
  that. Each repeat loses some top, the echoes can bounce left and right, and they duck under the voice.
- **Reverb** is an eight-line feedback delay network with a lossless mixing matrix, so the decay time is exact.
  Room, plate or hall. With **Timed to tempo** on, the pre-delay is a 64th note and the decay is one, two or four
  beats. **Fit to this track** chooses the space, length, tone and ducking from what the track is, the tempo and
  how bright it is, and says why; kick, 808 and finished beats are left dry.
- **Harmony** adds up to three voices on other notes of the key (third, fifth, octave, choir).
- **Shift** moves the whole voice up or down, as a layer or on its own, with control of the size of the voice.
- **Phone** is the voice through a small speaker. **Lo-fi** is sample-rate and bit reduction with tape wobble.
  **Motion** is chorus, flanger and phaser. **Chop** gates the sound in time with the beat.

A mono track can load the plugin as **Mono > Stereo**, which is what lets a mono vocal have wide effects.

**Low latency** (Settings) is for recording through the plugin. Every instance in the session drops its delaying
stages: Tune reacts in about 6 ms, and the master limiter and loudness boost are off. A badge in the header shows
it is on, and it is never saved with the song.

## AI tutor (optional, free)

Settings > **Turn on AI tutor** downloads the tutor once. After that an "Ask the AI tutor" box appears in the Mix
Brain panel on the EQ tab. The question goes to an AI model running on that Mac. Nothing leaves the machine, there
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
| EQ | Input and output analysers, 8 bands (bell, shelves, cuts to 48 dB/oct, notch), per-band stereo / mid / side, dynamic bands, band solo, auto gain, delta listening, Mix Brain suggestions, reference capture and match |
| Dynamics | Compressor with four styles (Clean, Punch, Smooth, Glue), sidechain high-pass, parallel mix, scrolling level and reduction display, transfer curve, Auto set; gate; de-esser |
| Tone | Smooth (resonance tamer), saturation (tape, tube, warm), level rider, Vocal space, stereo width and mono bass, output |
| Tune | Pitch correction: Simple (live, natural to hard tune, note hold, formant, custom notes) and Graph (capture a take, move each note by hand), Find key, Send key to all tracks |
| FX | Doubler, Delay, Reverb, Harmony, Shift, Phone, Lo-fi, Motion, Chop; presets for each, and combined Sounds |
| Master | Target modes (-18 / -14 / -9 LUFS), true-peak limiter with look-ahead, auto level, loudness history, release check, Mono and Phone speaker listening checks |
| Learn | Animated lessons: how sound works, what an EQ is, masking, what a compressor is, attack and release, gate and de-esser, saturation, stereo and mono, loudness and mastering, putting it together |

## What it does not do (yet)

- The EQ is minimum-phase and zero-latency. There is no linear-phase mode.
- Eight bands, not 24 or 32.
- Smooth works on 24 quarter-octave bands, not per FFT bin like a full spectral processor.
- No external sidechain input: instances share information through the session instead.
- The Mix Brain is a rule-based comparison against role tone curves and the other tracks. It is not a trained model.
  The AI tutor is separate, optional, and only explains; it never changes settings.
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
