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
2. On each one, pick a preset from the header (Vocals, Drums and bass, Instruments, Beat, Master) or just set the
   role under Settings. Presets fit their thresholds to how loud the track is, so load them while the song plays.
3. Put one on the Stereo Out and set its role to **Master**.
4. Play the loudest section. On the **Mix** tab press **Learn and balance** and keep it playing for 20 seconds.
   Every track's trim moves so the vocal leads and the mix lands at -18 LUFS with peaks under -6 dBTP.
5. Fix the **collisions** the Mix tab lists.
6. On the master's **Master** tab choose where the song is going (mix hand-off, streaming, loud) and read the
   release check.

Hover over any control and the bar along the bottom says what it does, when to use it and typical settings.
The **Learn** tab has ten lessons, each a few steps long. Every step has an animated example drawn from the same
maths the plugin uses, and several end with a "your turn" exercise.

## AI tutor (optional, free)

Settings > **Turn on AI tutor** downloads the tutor once (about 2 GB). After that an "Ask the AI tutor" box appears
in the Mix Brain panel on the EQ tab. The question goes to an AI model running on that Mac, together with this
track's measurements and a short fact sheet. Nothing leaves the machine, there is no account or key, and it works
without the internet. Answers come from a small model and can be wrong, which the plugin says under every answer.

How it works: the download is a helper program (the [llama.cpp](https://github.com/ggml-org/llama.cpp) server, MIT
licence, from that project's own Mac builds, pinned and checksummed in `scripts/make_tutor_pack.sh`) and one model
file (Llama 3.2 3B, named in `scripts/make_installer.sh`, fetched from Hugging Face and checksummed). They are saved
in `/Users/Shared/Musically Unorthodox`, so every account on a studio Mac shares one copy. The plugin starts the
helper when someone asks a question; it only listens on the Mac itself and gives its memory back when idle.
Needs macOS 13.3 or newer. Settings > Remove deletes it again.

Every release build installs the tutor on the build Mac and asks it two questions with the real model before
anything is published (`MUTutorCheck`), so a broken tutor never ships.

If the built-in tutor is not installed but [Ollama](https://ollama.com) is running with a chat model, the plugin uses that and no download
is needed.

## Tabs

| Tab | What is on it |
| --- | --- |
| Mix | Mix loudness dial, one lane per instance (level, target, move, where its energy is), collisions with one-click fixes, Learn and balance |
| EQ | Input and output analysers, 8 bands (bell, shelves, cuts to 48 dB/oct, notch), per-band stereo / mid / side, dynamic bands, band solo, auto gain, delta listening, Mix Brain suggestions, reference capture and match |
| Dynamics | Compressor with four styles (Clean, Punch, Smooth, Glue), sidechain high-pass, parallel mix, scrolling level and reduction display, transfer curve, Auto set; gate; de-esser |
| Tone | Smooth (resonance tamer), saturation (tape, tube, warm), level rider, Vocal space, stereo width and mono bass, output |
| Master | Target modes (-18 / -14 / -9 LUFS), true-peak limiter with look-ahead, auto level, loudness history, release check |
| Learn | Animated lessons: how sound works, what an EQ is, masking, what a compressor is, attack and release, gate and de-esser, saturation, stereo and mono, loudness and mastering, putting it together |

## What it does not do (yet)

- The EQ is minimum-phase and zero-latency. There is no linear-phase mode.
- Eight bands, not 24 or 32.
- Smooth works on 24 quarter-octave bands, not per FFT bin like a full spectral processor.
- No external sidechain input: instances share information through the session instead.
- The Mix Brain is a rule-based comparison against role tone curves and the other tracks. It is not a trained model.
  The AI tutor is separate, optional, and only explains; it never changes settings.
- It cannot change the settings of other plugins; it sits after them and corrects the result.

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
  analysis/TonalAnalysis.*       role tone curves, suggestions, clash wording, EQ match
  session/SessionBus.*           process-wide bus shared by all instances, learn and balance, shared reference
  session/BalanceEngine.*        role targets, balance plan, masking detection
  update/Updater.*               in-plugin updates: feed check, download, checksum, in-place install
  ai/LocalAI.*                   optional AI tutor: built-in helper and model, or a local Ollama server
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
