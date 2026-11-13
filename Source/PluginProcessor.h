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

namespace mu
{
// What the output carries. The delta modes let you hear only what a module is changing.
enum class Listen : int { Normal = 0, EqDelta, SmoothDelta };

// Signal chain:
//   trim -> gate -> EQ -> compressor -> de-esser -> smooth -> saturation -> rider -> vocal space
//        -> stereo -> output -> [master only: auto trim -> maximizer] -> metering
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
    double getTailLengthSeconds() const override { return 0.0; }
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
    void setSessionInstanceNumber (int n) override { instanceNumber = n; }
    void resetMeasurement() override { loudness.resetIntegrated(); loudness.resetPeakHold(); mixMeter.resetIntegrated(); mixMeter.resetPeakHold(); }

    // Access for the editor
    juce::AudioProcessorValueTreeState apvts;
    SessionBus& getBus() { return *bus; }
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MUProcessor)
};
}
