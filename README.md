# Musically Unorthodox mix assistant

The MU studio's in-house mixing and mastering plugin. One instance goes on every track and one on the Stereo Out;
they talk to each other inside the project. Together they balance the session against the lead vocal, show where
tracks are fighting for the same frequencies, and take the finished mix to a release loudness. Every instance also
carries a full channel strip (gate, 8-band EQ, compressor, de-esser, resonance smoothing, saturation, level rider,
stereo tools) and explains each control as you use it.

Internal tool for Musically Unorthodox (In Music In Media C.I.C., Northampton). Not for public distribution.
Everything runs on the computer; nothing is sent anywhere.

## Getting the installer

Every push to `main` builds the AU, VST3 and standalone on a Mac runner, runs the tests and Apple's `auval`,
and attaches a double-click `.pkg` to a Release on the repo page. Download the latest `.pkg`, run it, restart Logic.
If macOS says it can't be opened: right-click the .pkg > Open > Open (it is not notarised).

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
The **Learn** tab has ten short lessons with diagrams you can drag.

## Tabs

| Tab | What is on it |
| --- | --- |
| Mix | Mix loudness dial, one lane per instance (level, target, move, where its energy is), collisions with one-click fixes, Learn and balance |
| EQ | Input and output analysers, 8 bands (bell, shelves, cuts to 48 dB/oct, notch), per-band stereo / mid / side, dynamic bands, band solo, auto gain, delta listening, Mix Brain suggestions, reference capture and match |
| Dynamics | Compressor with four styles (Clean, Punch, Smooth, Glue), sidechain high-pass, parallel mix, scrolling level and reduction display, transfer curve, Auto set; gate; de-esser |
| Tone | Smooth (resonance tamer), saturation (tape, tube, warm), level rider, Vocal space, stereo width and mono bass, output |
| Master | Target modes (-18 / -14 / -9 LUFS), true-peak limiter with look-ahead, auto level, loudness history, release check |
| Learn | Lessons: mix order, the frequency map, EQ, compression, attack and release, gates and de-essers, saturation, loudness, the vocal chain, vocals over a two-track beat |

## What it does not do (yet)

- The EQ is minimum-phase and zero-latency. There is no linear-phase mode.
- Eight bands, not 24 or 32.
- Smooth works on 24 quarter-octave bands, not per FFT bin like a full spectral processor.
- No external sidechain input: instances share information through the session instead.
- The Mix Brain is a rule-based comparison against role tone curves and the other tracks. It is not a trained model.
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
  ui/                            theme, icons, look and feel, widgets, help text, pages
  tests/TestMain.cpp             DSP and analysis tests
  tests/SessionHarness.cpp       4-instance offline session test
  tests/PreviewHarness.cpp       live UI preview fed with synthetic stems
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
