#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "state/ParameterIDs.h"
#include "state/ParameterLayout.h"
#include "state/ChainPresets.h"
#include "dsp/LoudnessMeter.h"
#include "dsp/SpectrumAnalyzer.h"
#include "dsp/EQEngine.h"
#include "dsp/Compressor.h"
#include "dsp/Gate.h"
#include "dsp/DeEsser.h"
#include "dsp/Smoother.h"
#include "dsp/Saturator.h"
#include "dsp/Rider.h"
#include "dsp/StereoTool.h"
#include "dsp/Maximizer.h"
#include "dsp/VocalSpace.h"
#include "dsp/RoleDetector.h"
#include "dsp/History.h"
#include "analysis/TonalAnalysis.h"
#include "session/SessionBus.h"
#include "dsp/Tuner.h"
#include "dsp/Doubler.h"
#include "dsp/Reverb.h"
#include "dsp/Echo.h"
#include "dsp/PitchFX.h"
#include "dsp/ColourFX.h"
#include "dsp/PeakGuard.h"
#include "dsp/KeyFinder.h"
#include "dsp/TuneGraph.h"
#include "analysis/ReverbAdvisor.h"
#include "state/ModulePresets.h"
#include "update/Updater.h"
#include "ai/LocalAI.h"
#include "state/LocalSettings.h"

namespace mu
{
// What the output carries. The delta modes let you hear only what a module is changing.
enum class Listen : int { Normal = 0, EqDelta, SmoothDelta };

// Listening checks on the master: how the mix sounds on other speakers. They change only what is heard, never
// the meters, and they switch themselves off for a bounce.
enum class ListenCheck : int { Off = 0, Mono, Phone };

// One line of the bounce check.
struct CheckItem
{
    enum State { Good = 0, Warn, Problem, Info };
    State state = Good;
    juce::String title, detail;
};

// Signal chain:
//   trim -> tune -> gate -> EQ -> compressor -> de-esser -> smooth -> saturation -> rider -> vocal space
//        -> doubler + harmony + shift (side by side, all hearing the clean voice) -> phone -> lo-fi -> motion -> chop
//        -> delay -> reverb -> stereo -> output -> [tracks: peak guard] [master: auto trim -> maximizer] -> metering
class MUProcessor : public juce::AudioProcessor,
                    public SessionClient,
                    private juce::Timer
{
public:
    static constexpr int kLoudnessHistory = 600;   // 60 s of short-term loudness, one point per 100 ms

    MUProcessor();
    ~MUProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Musically Unorthodox"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override
    {
        const double rv = getParam (ids::rvOn) > 0.5f ? (double) reverb.currentDecaySeconds() : 0.0;
        const double dl = getParam (ids::dlOn) > 0.5f ? (double) echo.tailSeconds() : 0.0;
        return juce::jmax (rv, dl);
    }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    void updateTrackProperties (const TrackProperties& properties) override;

    // SessionClient
    juce::String getDisplayName() const override;
    Role getEffectiveRole() const override;
    float getTrimDb() const override;
    void setTrimDb (float db) override;
    bool acceptsBalance() const override;
    void setAcceptsBalance (bool on) override { setParam (ids::autoBalance, on ? 1.0f : 0.0f); }
    bool fillBandsDb (std::array<float, kAnalysisBands>& out) const override;
    void applyMaskingCut (float hz, float db) override;
    float cutDbNear (float hz) const override;
    void setSessionInstanceNumber (int n) override { instanceNumber = n; }
    void resetMeasurement() override { loudness.resetIntegrated(); loudness.resetPeakHold(); mixMeter.resetIntegrated(); mixMeter.resetPeakHold(); guard.resetInputPeak(); }
    KeyEstimate getMusicKey() const override { return keyFinder.estimate(); }
    bool getNameKey (int& key, bool& minor) const override { return keyFromName (trackName, key, minor); }
    void setSongKey (int key, bool minor) override;
    bool isTuneOn() const override { return getParam (ids::tuneOn) > 0.5f; }
    float getPeakOverDb() const override;
    bool isCompressorOn() const override { return getParam (ids::compEnabled) > 0.5f; }
    juce::String tamePeaks() override;
    void turnOutputDown (float db) override { setParam (ids::outputGain, juce::jlimit (-24.0f, 24.0f, getParam (ids::outputGain) - db)); }
    float getStereoCorrelation() const override { return getTotalNumOutputChannels() > 1 ? stereo.getCorrelation() : 1.0f; }

    // Access for the editor
    juce::AudioProcessorValueTreeState apvts;
    SessionBus& getBus() { return *bus; }
    Updater& getUpdater() { return *updater; }
    LocalAI& getAI() { return *ai; }
    LocalSettings& getSettings() { return *settings; }
    juce::String tutorPrompt (const juce::String& question);   // everything the AI tutor is told before it answers
    juce::String tutorTrackNotes();                            // the measured part: this track and the session
    juce::StringPairArray tutorFacts();                        // measurements turned into finished sentences for the guide notes
    const LoudnessMeter& getLoudness() const { return loudness; }                // what leaves the plugin
    const LoudnessMeter& getMixMeter() const { return mixMeter; }                // master only: the mix before the mastering stage
    const SpectrumAnalyzer& getSpectrum() const { return spectrum; }          // after the chain
    const SpectrumAnalyzer& getPreSpectrum() const { return preSpectrum; }    // before the chain
    const EQEngine& getEQ() const { return eq; }
    const Compressor& getCompressor() const { return comp; }
    const Gate& getGate() const { return gate; }
    const DeEsser& getDeEsser() const { return deEsser; }
    const Smoother& getSmoother() const { return smoother; }
    const Saturator& getSaturator() const { return saturator; }
    const Rider& getRider() const { return rider; }
    const StereoTool& getStereo() const { return stereo; }
    const Maximizer& getMaximizer() const { return maximizer; }
    const VocalSpace& getVocalSpace() const { return vocalSpace; }
    Tuner& getTuner() { return tuner; }
    const Doubler& getDoubler() const { return doubler; }
    const Reverb& getReverb() const { return reverb; }
    double getHostBpm() const { return hostBpm.load(); }
    bool hasHostTempo() const { return hostTempoKnown.load(); }
    bool isStereoOut() const { return getTotalNumOutputChannels() > 1; }
    const Echo& getEcho() const { return echo; }
    const PitchFX& getPitchFX() const { return pitchFx; }
    const Phone& getPhone() const { return phone; }
    const Motion& getMotion() const { return motion; }
    const Chop& getChop() const { return chop; }
    const PeakGuard& getGuard() const { return guard; }
    juce::String findKey();            // sets the Tune key and scale: from the session if it knows the key, else from the notes sung
    SongKey songKey() const;           // what findKey would use, without changing anything
    juce::String sendKeyToSession();   // every instance in the session takes this instance's key and scale
    juce::String fitReverb();          // chooses a reverb for this track from its role, the tempo and how it sounds
    ReverbFacts reverbFacts() const;

    // Tune's graph view
    TuneGraph& getGraph() { return graph; }
    juce::CriticalSection& getGraphLock() { return graphLock; }
    void startCapture();
    void stopCapture();
    bool isCapturing() const { return graph.isCapturing(); }
    juce::String correctAllNotes (float straighten01);      // every note to the nearest note of the scale
    void resetAllNotes();
    void clearGraph();
    bool isHostPlaying() const { return hostPlaying.load (std::memory_order_relaxed); }
    double getHostSeconds() const { return hostSeconds.load (std::memory_order_relaxed); }

    // Presets for one tool, and complete sounds
    juce::String applyModulePreset (int index);
    juce::String applySound (int index);
    juce::String getModulePresetName (const juce::String& module) const;
    juce::String getSoundName() const { return apvts.state.getProperty ("soundName", juce::String()).toString(); }

    // Listening checks and the bounce check
    void setListenCheck (ListenCheck c) { listenCheck.store ((int) c); }
    ListenCheck getListenCheck() const { return (ListenCheck) listenCheck.load(); }
    std::vector<CheckItem> bounceCheck();
    RoleGuess guessRole (const RoleFeatures& f) const;
    const History<kLoudnessHistory>& getLoudnessHistory() const { return lufsHistory; }
    RoleGuess getDetectedRole() const { return detected; }
    int getSessionSlot() const { return slot; }
    int getInstanceNumber() const { return instanceNumber; }
    juce::String getTrackName() const { return trackName; }
    bool isMasterInstance() const { return getEffectiveRole() == Role::Master; }
    float getMasterAutoTrimDb() const { return masterTrimDb.load(); }
    bool isAutoLevelHeld() const { return autoLevelHeld.load(); }     // the limiter is at its sensible maximum, so the target was not chased further
    juce::String getCurrentPresetName() const { return currentPreset; }
    bool isPlaying() const { return loudness.isActive(); }

    // Analysis pump: the editor calls this every frame so the analyser moves smoothly;
    // the processor's own timer calls it when no editor is open.
    void pumpAnalysis();
    void editorOpened() { editorsOpen.fetch_add (1); }
    void editorClosed() { editorsOpen.fetch_sub (1); }

    // Listening
    void setListenMode (Listen m) { listenMode.store ((int) m); }
    Listen getListenMode() const { return (Listen) listenMode.load(); }
    void setSoloBand (int band) { soloBand = band; eq.setSoloBand (band); }
    int getSoloBand() const { return soloBand; }

    // A/B: two complete settings snapshots to flip between.
    void switchAB();
    void copyToOtherAB();
    int getABSlot() const { return abSlot; }

    // Mix Brain
    std::vector<Suggestion> analyse (juce::String& status);
    juce::String applySuggestion (const Suggestion& s);
    float secondsHeard() const;

    // Reference tone (shared across the session)
    juce::String captureReference();
    juce::String matchReference (float amount01);
    bool hasReference (juce::String& sourceName) const;
    bool getReferenceBands (BandArray& out) const;

    // Actions (message thread)
    void setAutoBalanceMode (bool on);
    bool getAutoBalanceMode() const { return autoBalanceMode; }
    void balanceNow();
    void learnAndBalance (float seconds = 20.0f);
    juce::String autoCompressor();     // sets the compressor from role + measured level
    juce::String applyChainPreset (int index);
    void applyMasterMode (int mode);
    void resetEQ();
    void resetTrim();
    void resetMeters();
    void setParam (const juce::String& id, float value);
    float getParam (const juce::String& id) const;
    juce::String getParamText (const juce::String& id) const;
    int getCurrentTab() const { return currentTab; }
    void setCurrentTab (int t) { currentTab = t; }
    BalanceTargets currentTargets() const;

    EQBandSettings getBandSettings (int band) const;   // from the parameters (safe when audio is not running)
    void setBandSettings (int band, const EQBandSettings& s);
    int pickBandFor (const EQBandSettings& s) const;
    float eqMagnitudeDbAt (float hz) const;
    float eqBandMagnitudeDbAt (int band, float hz) const;

    static juce::Colour roleColour (Role r);

private:
    void timerCallback() override;
    void pullParameters();
    void updateLatency();
    void fitThresholds (Role presetRole, juce::String& note);
    std::vector<float> captureSnapshot() const;
    void applySnapshot (const std::vector<float>& values);

    juce::SharedResourcePointer<SessionBus> bus;
    juce::SharedResourcePointer<Updater> updater;
    juce::SharedResourcePointer<LocalAI> ai;
    juce::SharedResourcePointer<LocalSettings> settings;
    int slot = -1;
    int instanceNumber = 0;
    juce::String trackName;
    std::atomic<int> currentTab { 0 };
    bool autoBalanceMode = false;
    juce::String currentPreset;
    std::atomic<int> editorsOpen { 0 };
    std::atomic<int> listenMode { 0 };
    int soloBand = -1;

    LoudnessMeter loudness, mixMeter;
    SpectrumAnalyzer spectrum, preSpectrum;
    Gate gate;
    EQEngine eq;
    Compressor comp;
    DeEsser deEsser;
    Smoother smoother;
    Saturator saturator;
    Rider rider;
    VocalSpace vocalSpace;
    StereoTool stereo;
    Maximizer maximizer;
    Tuner tuner;
    Doubler doubler;
    Reverb reverb;
    Echo echo;
    PitchFX pitchFx;
    Phone phone;
    LoFi lofi;
    Motion motion;
    Chop chop;
    PeakGuard guard;
    KeyFinder keyFinder;
    TuneGraph graph;
    mutable juce::CriticalSection graphLock;
    juce::AudioBuffer<float> cleanBuffer;
    std::atomic<bool> hostPlaying { false };
    std::atomic<double> hostSeconds { 0.0 };
    std::atomic<int> listenCheck { 0 };
    ColourBiquad checkHp1, checkHp2, checkLp;
    int checkWas = 0, captureFrom = -1;
    bool captureSeen = false;
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostTempoKnown { false };
    juce::AudioBuffer<float> deltaBuffer;
    juce::SmoothedValue<float> trimGain { 1.0f }, outGain { 1.0f }, masterTrimGain { 1.0f };
    std::atomic<float> masterTrimDb { 0.0f };
    std::atomic<bool> autoLevelHeld { false };
    double lastAutoTrimStep = 0.0;     // seconds of processed audio
    float lastGuardState = -1000.0f;
    std::atomic<uint64_t> samplesProcessed { 0 };
    int reportedLatency = -1;
    RoleGuess detected;
    std::atomic<int> detectedRoleAtomic { (int) Role::Beat };
    History<kLoudnessHistory> lufsHistory;

    std::vector<juce::RangedAudioParameter*> abParams;
    std::array<std::vector<float>, 2> abStates;
    int abSlot = 0;

    // cached parameter pointers
    using AF = std::atomic<float>*;
    AF pRole = nullptr, pBypass = nullptr, pTrim = nullptr, pOut = nullptr, pAutoBal = nullptr;
    AF pTargetLufs = nullptr, pTargetTp = nullptr, pMasterAutoTrim = nullptr, pPeakGuard = nullptr, pMaxGain = nullptr;
    std::array<AF, (size_t) Role::NumRoles> pRoleOff {};
    AF pGateOn = nullptr, pGateThr = nullptr, pGateRange = nullptr, pGateAtt = nullptr, pGateRel = nullptr, pGateHold = nullptr;
    AF pEqOn = nullptr, pEqAutoGain = nullptr;
    struct BandPtrs { AF on, type, freq, gain, q, dyn, thr, range, slope, mode; };
    std::array<BandPtrs, kNumEQBands> pBands {};
    AF pCompOn = nullptr, pThr = nullptr, pRatio = nullptr, pAtt = nullptr, pRel = nullptr, pKnee = nullptr, pMakeup = nullptr, pAutoMakeup = nullptr, pMix = nullptr, pCompStyle = nullptr, pCompScHp = nullptr;
    AF pDsOn = nullptr, pDsFreq = nullptr, pDsThr = nullptr, pDsRange = nullptr, pDsMode = nullptr;
    AF pSmOn = nullptr, pSmDepth = nullptr, pSmFocus = nullptr, pSmSpeed = nullptr;
    AF pSatOn = nullptr, pSatDrive = nullptr, pSatMix = nullptr, pSatType = nullptr;
    AF pRiderOn = nullptr, pRiderTarget = nullptr, pRiderRange = nullptr, pRiderSpeed = nullptr;
    AF pVsOn = nullptr, pVsAmount = nullptr, pVsSpeed = nullptr;
    AF pStWidth = nullptr, pStMonoBass = nullptr, pStMonoFreq = nullptr;
    AF pTuneOn = nullptr, pTuneKey = nullptr, pTuneScale = nullptr, pTuneSpeed = nullptr, pTuneAmount = nullptr, pTuneVibrato = nullptr;
    AF pDblOn = nullptr, pDblMix = nullptr, pDblDetune = nullptr, pDblTiming = nullptr, pDblWidth = nullptr, pDblVoices = nullptr;
    AF pRvOn = nullptr, pRvMix = nullptr, pRvAuto = nullptr, pRvLength = nullptr, pRvDecay = nullptr, pRvPre = nullptr, pRvTone = nullptr, pRvDuck = nullptr;
    AF pRvType = nullptr, pRvLowCut = nullptr, pRvWidth = nullptr;
    AF pTuneMode = nullptr, pTuneHold = nullptr, pTuneFormant = nullptr, pTuneNotes = nullptr, pTrackGuard = nullptr;
    AF pDlOn = nullptr, pDlMix = nullptr, pDlSync = nullptr, pDlNote = nullptr, pDlTime = nullptr, pDlFeedback = nullptr, pDlTone = nullptr, pDlWidth = nullptr, pDlDuck = nullptr;
    AF pHmOn = nullptr, pHmVoicing = nullptr, pHmMix = nullptr, pHmWidth = nullptr;
    AF pPsOn = nullptr, pPsSemis = nullptr, pPsCharacter = nullptr, pPsMix = nullptr;
    AF pPhOn = nullptr, pPhAmount = nullptr, pPhTone = nullptr, pPhDrive = nullptr;
    AF pLfOn = nullptr, pLfCrush = nullptr, pLfWobble = nullptr, pLfMix = nullptr;
    AF pMdOn = nullptr, pMdType = nullptr, pMdRate = nullptr, pMdDepth = nullptr, pMdMix = nullptr;
    AF pChOn = nullptr, pChRate = nullptr, pChDepth = nullptr, pChShape = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MUProcessor)
};
}
