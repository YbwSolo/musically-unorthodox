# Musically Unorthodox — mix assistant

A plugin you put on every track plus the master. The instances talk to each other inside the DAW,
work out how loud each track should be against the vocal, and move each track's level so the whole
song lands at a hand-off level a mastering engineer wants (-18 LUFS integrated, -6 dBTP by default).
It also carries its own EQ, compressor and a "Vocal Space" ducker so the beat gets out of the vocal's way.

Nothing runs in the cloud. No account, no credits.

## Easiest: let GitHub build it

Push this folder to a GitHub repo. The workflow in `.github/workflows/build-mac.yml` builds the AU, VST3 and
standalone on a Mac runner, runs the tests and `auval`, and attaches a double-click `.pkg` installer to a
Release on the repo page. Download the `.pkg`, run it, restart Logic. No Xcode needed on your own Mac.

## Build on the Mac yourself

1. Install Xcode command line tools and CMake:
   `xcode-select --install` then `brew install cmake`
2. In Terminal: `bash scripts/build_mac.sh`

That fetches JUCE 8 from GitHub the first time, builds the AU, VST3 and standalone (universal),
runs the DSP tests, runs `auval`, and installs the plugin into `~/Library/Audio/Plug-Ins`.
Logic shows it under Audio Units > Musically Unorthodox.

To open the project in Xcode instead: `open build-mac/MusicallyUnorthodox.xcodeproj`

Windows later: same CMake project, `cmake -B build -G "Visual Studio 17 2022"` then build the VST3 target.

## How to use it

1. Put an instance **last** on each track (after the plugins the artist already mixed with).
2. Set the **Role** at the top right: Vocal, Beat, Kick, 808 and so on. Auto detect guesses after a
   few seconds of playback, but setting it yourself gives the best balance.
3. Put one instance on the **master bus** and set its role to **Master**. That one runs the session.
4. Play the song through once so every instance has listened, then on the master's **Mix** tab
   hit **BALANCE NOW**. Each track's trim moves so the mix sits at the target.
5. The **Levels** tab on the master tells you when the song is ready to hand to mastering.

### Tabs

- **Mix** — session table (every instance, its level, peak, trim and next move), target loudness
  and true-peak ceiling, Balance Now, Auto balance, masking clashes.
- **EQ** — 6-band parametric with spectrum. Double-click to add a band, drag, wheel for Q,
  alt-click to remove. Clean Harsh finds persistent harsh resonances and cuts them.
  Vocal Space (on beat/instrument instances) ducks 260 Hz / 2.6 kHz / 7 kHz only while the vocal is singing.
- **Compressor** — threshold, ratio, attack, release, knee, makeup, mix, auto makeup, GR meter.
  Auto Set picks settings from the track's role and measured level.
- **Levels** — momentary / short-term / integrated LUFS, true peak, crest factor, readiness checklist.
- **Settings** — licence, what's free and what's Pro.

### Why it can't edit Waves / FabFilter settings directly

Plugins cannot touch other plugins' parameters; the host owns them. So this sits after their chain and
corrects the result (level, resonances, masking, dynamics). Same outcome, works with any plugin.

## Free vs Pro

Free: session view, role detection, LUFS/true-peak metering, Balance Now, manual EQ and compressor.
Pro: Auto balance (continuous), masking fixes, Clean Harsh, Vocal Space, compressor Auto Set.

Gates live in `Source/license/License.h` (`features::`) and are checked in the UI pages, so moving
a feature between tiers is a one-line change.

Keys: `python3 scripts/make_key.py buyer@email.com` prints the key for that email. The buyer enters
email + key on the Settings tab. The secret is in both `make_key.py` and `License.cpp`; keep them identical.
This is an offline scheme (fine for launch, swap for a server check later).

## Project layout

```
CMakeLists.txt                 JUCE 8 project (AU, VST3, Standalone, tests)
Assets/mu_logo_white.png       logo baked into the binary
scripts/build_mac.sh           one-shot Mac build + auval
scripts/make_key.py            licence key generator
Source/
  PluginProcessor.*            audio chain, parameters, actions, state
  PluginEditor.*               window + tab switching
  state/ParameterIDs.h         every parameter id (don't rename after release)
  state/ParameterLayout.*      ranges, defaults, display
  dsp/LoudnessMeter.*          BS.1770 LUFS + true peak
  dsp/SpectrumAnalyzer.*       FFT display, long-term bands, resonance finder
  dsp/EQEngine.*               6-band biquad EQ (allocation-free updates)
  dsp/Compressor.*             feed-forward compressor
  dsp/VocalSpace.*             vocal-keyed dynamic EQ for beats
  dsp/RoleDetector.*           instrument role heuristics
  session/SessionBus.*         process-wide bus shared by all instances
  session/BalanceEngine.*      role targets, balance plan, masking detection
  license/License.*            offline key check + feature gates
  ui/                          header, pages, look and feel, meters
  tests/TestMain.cpp           41 DSP/engine tests (MUTests)
  tests/SessionHarness.cpp     4-instance offline session test (MUSession)
```

## Notes

- All instances must live in the same host process to see each other (normal for Logic, Ableton,
  FL, Reaper, Cubase, Studio One). If a host sandboxes each plugin separately, the session table only
  shows the current instance.
- Track names come from the host when it provides them (VST3 hosts do; Logic AU doesn't), otherwise
  instances are named by role and number.
- Parameter IDs are versioned (`ParameterID { id, 1 }`). Never rename them once sessions are saved.
- Oversampled true peak, K-weighting and the gating are tested against known values in `MUTests`.
