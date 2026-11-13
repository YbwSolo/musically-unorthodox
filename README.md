# Musically Unorthodox — mix assistant

The MU studio's in-house mixing plugin. One instance goes on every track and one on the master; they talk to
each other inside the project, work out how loud each track should be against the lead vocal, and move each
track so the whole song lands at the hand-off level a mastering engineer wants. Each instance also carries the
studio vocal chain: gate, dynamic EQ, compressor, de-esser, saturation, level rider, and Vocal Space for beats.

Internal tool for Musically Unorthodox (In Music In Media C.I.C., Northampton). Not for public distribution.
Everything runs on the computer; nothing is sent anywhere.

## Getting the installer

Every push to `main` builds the AU, VST3 and standalone on a Mac runner, runs the tests and Apple's `auval`,
and attaches a double-click `.pkg` to a Release on the repo page. Download the latest `.pkg`, run it, restart Logic.
If macOS says it can't be opened: right-click the .pkg > Open > Open (it is not notarised).

Logic: Audio Units > Musically Unorthodox. Also installs the VST3 and a standalone app in Applications.

## Using it

1. Put Musically Unorthodox **last** on every track, after the plugins the song was mixed with.
2. On each one load a **CHAIN** from the header (MU Lead Vocal, MU 808, MU Beat...) or just set the **ROLE**.
3. Put one on the Stereo Out and load **MU Master Prep**. That instance runs the session.
4. Play the loudest section. On the master's **MIX** tab press **LEARN & BALANCE** and keep it playing for 20 s.
5. Every track's trim moves so the vocal sits on top and the song lands on the target. The master's **LEVELS**
   tab tells you when it is ready to bounce for mastering.

Tabs: MIX (session, targets, learn/balance, masking), EQ (6 bands, any of them dynamic, Clean Harsh),
COMP (compressor + gate, Auto Set), CHAIN (de-esser, saturation, rider, vocal space), LEVELS, HELP, SETTINGS.
The HELP tab inside the plugin explains all of it for students.

## Building on a Mac by hand

`xcode-select --install`, `brew install cmake`, then `bash scripts/build_mac.sh`. Fetches JUCE 8 the first time,
builds universal binaries, runs tests + auval, installs to `~/Library/Audio/Plug-Ins`.
`bash scripts/make_installer.sh` packages a `.pkg` from a build.

## Project layout

```
CMakeLists.txt                 JUCE 8 project (AU, VST3, Standalone, MUTests, MUSession)
Assets/                        logo + Anton / Inter fonts baked into the binary
scripts/build_mac.sh           one-shot Mac build + auval
scripts/make_installer.sh      .pkg installer
.github/workflows/build-mac.yml  CI: Mac build -> tests -> auval -> pkg -> Release
Source/
  PluginProcessor.*            chain, parameters, actions, state, master auto-trim
  PluginEditor.*               window, tabs, toast
  state/ParameterIDs.h         every parameter id (never rename after release)
  state/ParameterLayout.*      ranges, defaults, display
  state/ChainPresets.*         MU chain presets (from the studio Logic template)
  dsp/LoudnessMeter.*          BS.1770 LUFS + true peak
  dsp/SpectrumAnalyzer.*       FFT display, long-term bands, resonance finder
  dsp/EQEngine.*               6-band EQ with dynamic bands
  dsp/Compressor.*  Gate.*  DeEsser.*  Saturator.*  Rider.*  PeakGuard.*  VocalSpace.*
  dsp/RoleDetector.*           instrument role heuristics
  session/SessionBus.*         process-wide bus shared by all instances, learn & balance
  session/BalanceEngine.*      role targets, balance plan, masking detection
  ui/                          brand theme, look and feel, widgets, pages
  tests/TestMain.cpp           DSP / engine tests
  tests/SessionHarness.cpp     4-instance offline session test
```

## Notes

- Signal chain per instance: trim > gate > EQ > compressor > de-esser > saturation > rider > vocal space >
  output > (master only) auto trim > peak guard. Latency is fixed per instance (saturation oversampling +
  peak-guard look-ahead) and reported to the host.
- All instances must be in the same host process to see each other (normal for Logic, Ableton, FL, Reaper, Cubase).
- Track names come from hosts that provide them (VST3 hosts do; Logic AU does not), otherwise role + number.
- Role targets (LU relative to the lead vocal) live on the master instance's parameters and save with the project.
- Brand: black / white / MU yellow #FBF20D, Anton for headlines, Inter for text (both OFL, bundled).
