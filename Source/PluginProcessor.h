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
#include "dsp/Saturator.h"
#include "dsp/Rider.h"
#include "dsp/PeakGuard.h"
#include "dsp/VocalSpace.h"
#include "dsp/RoleDetector.h"
#include "session/SessionBus.h"

namespace mu
{
// Signal chain: trim -> gate -> EQ (static + dynamic) -> compressor -> de-esser -> saturation -> rider
//               -> vocal space -> output -> [master only: auto trim -> peak guard] -> metering
class MUProcessor : public juce::AudioProcessor,
                    public SessionClient,
                    private juce::Timer
{
public:
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
    bool fillBandsDb (std::array<float, kAnalysisBands>& out) const override;
    void applyMaskingCut (float hz, float db) override;
    void setSessionInstanceNumber (int n) override { instanceNumber = n; }
    void resetMeasurement() override { loudness.resetIntegrated(); loudness.resetPeakHold(); }

    // Access for the editor
    juce::AudioProcessorValueTreeState apvts;
    SessionBus& getBus() { return *bus; }
    const LoudnessMeter& getLoudness() const { return loudness; }
    const SpectrumAnalyzer& getSpectrum() const { return spectrum; }
    const EQEngine& getEQ() const { return eq; }
    const Compressor& getCompressor() const { return comp; }
    const Gate& getGate() const { return gate; }
    const DeEsser& getDeEsser() const { return deEsser; }
    const Saturator& getSaturator() const { return saturator; }
    const Rider& getRider() const { return rider; }
    const PeakGuard& getPeakGuard() const { return peakGuard; }
    const VocalSpace& getVocalSpace() const { return vocalSpace; }
    RoleGuess getDetectedRole() const { return detected; }
    int getSessionSlot() const { return slot; }
    int getInstanceNumber() const { return instanceNumber; }
    juce::String getTrackName() const { return trackName; }
    bool isMasterInstance() const { return getEffectiveRole() == Role::Master; }
    float getMasterAutoTrimDb() const { return masterTrimDb.load(); }
    int getCurrentPresetIndex() const { return currentPreset; }

    // Actions (message thread)
    void setAutoBalanceMode (bool on);
    bool getAutoBalanceMode() const { return autoBalanceMode; }
    void balanceNow();
    void learnAndBalance (float seconds = 20.0f);
    juce::String cleanHarsh();         // sets EQ bands from detected resonances
    juce::String autoCompressor();     // sets compressor from role + measured level
    juce::String applyChainPreset (int index);
    void resetEQ();
    void resetTrim();
    void resetMeters();
    void setParam (const juce::String& id, float value);
    float getParam (const juce::String& id) const;
    int getCurrentTab() const { return currentTab; }
    void setCurrentTab (int t) { currentTab = t; }
    BalanceTargets currentTargets() const;

    EQBandSettings getBandSettings (int band) const;   // from the parameters (safe when audio is not running)
    float eqMagnitudeDbAt (float hz) const;
    float eqBandMagnitudeDbAt (int band, float hz) const;

    static juce::Colour roleColour (Role r);

private:
    void timerCallback() override;
    void pullParameters();
    void updateLatency();

    juce::SharedResourcePointer<SessionBus> bus;
    int slot = -1;
    int instanceNumber = 0;
    juce::String trackName;
    std::atomic<int> currentTab { 0 };
    bool autoBalanceMode = false;
    int currentPreset = -1;

    LoudnessMeter loudness;
    SpectrumAnalyzer spectrum;
    Gate gate;
    EQEngine eq;
    Compressor comp;
    DeEsser deEsser;
    Saturator saturator;
    Rider rider;
    VocalSpace vocalSpace;
    PeakGuard peakGuard;
    juce::SmoothedValue<float> trimGain { 1.0f }, outGain { 1.0f }, masterTrimGain { 1.0f };
    std::atomic<float> masterTrimDb { 0.0f };
    double lastAutoTrimStepMs = 0.0;   // in seconds of processed audio
    std::atomic<uint64_t> samplesProcessed { 0 };
    int reportedLatency = -1;
    RoleGuess detected;
    std::atomic<int> detectedRoleAtomic { (int) Role::Beat };

    // cached parameter pointers
    using AF = std::atomic<float>*;
    AF pRole = nullptr, pBypass = nullptr, pTrim = nullptr, pOut = nullptr, pAutoBal = nullptr;
    AF pTargetLufs = nullptr, pTargetTp = nullptr, pMasterAutoTrim = nullptr, pPeakGuard = nullptr;
    std::array<AF, (size_t) Role::NumRoles> pRoleOff {};
    AF pGateOn = nullptr, pGateThr = nullptr, pGateRange = nullptr, pGateAtt = nullptr, pGateRel = nullptr, pGateHold = nullptr;
    AF pEqOn = nullptr;
    struct BandPtrs { AF on, type, freq, gain, q, dyn, thr, range; };
    std::array<BandPtrs, kNumEQBands> pBands {};
    AF pCompOn = nullptr, pThr = nullptr, pRatio = nullptr, pAtt = nullptr, pRel = nullptr, pKnee = nullptr, pMakeup = nullptr, pAutoMakeup = nullptr, pMix = nullptr;
    AF pDsOn = nullptr, pDsFreq = nullptr, pDsThr = nullptr, pDsRange = nullptr, pDsMode = nullptr;
    AF pSatOn = nullptr, pSatDrive = nullptr, pSatMix = nullptr, pSatType = nullptr;
    AF pRiderOn = nullptr, pRiderTarget = nullptr, pRiderRange = nullptr, pRiderSpeed = nullptr;
    AF pVsOn = nullptr, pVsAmount = nullptr, pVsSpeed = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MUProcessor)
};
}
