#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "state/ParameterIDs.h"
#include "state/ParameterLayout.h"
#include "dsp/LoudnessMeter.h"
#include "dsp/SpectrumAnalyzer.h"
#include "dsp/EQEngine.h"
#include "dsp/Compressor.h"
#include "dsp/VocalSpace.h"
#include "dsp/RoleDetector.h"
#include "session/SessionBus.h"

namespace mu
{
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

    // Access for the editor
    juce::AudioProcessorValueTreeState apvts;
    SessionBus& getBus() { return *bus; }
    const LoudnessMeter& getLoudness() const { return loudness; }
    const SpectrumAnalyzer& getSpectrum() const { return spectrum; }
    const EQEngine& getEQ() const { return eq; }
    const Compressor& getCompressor() const { return comp; }
    const VocalSpace& getVocalSpace() const { return vocalSpace; }
    RoleGuess getDetectedRole() const { return detected; }
    int getSessionSlot() const { return slot; }
    int getInstanceNumber() const { return instanceNumber; }
    juce::String getTrackName() const { return trackName; }
    bool isMasterInstance() const { return getEffectiveRole() == Role::Master; }

    // Actions (message thread)
    void setAutoBalanceMode (bool on);
    bool getAutoBalanceMode() const { return autoBalanceMode; }
    void balanceNow();
    juce::String cleanHarsh();         // sets EQ bands from detected resonances
    juce::String autoCompressor();     // sets compressor from role + measured level
    void resetEQ();
    void resetTrim();
    void resetMeters();
    int getCurrentTab() const { return currentTab; }
    void setCurrentTab (int t) { currentTab = t; }

    EQBandSettings getBandSettings (int band) const;   // from the parameters (safe when audio is not running)
    float eqMagnitudeDbAt (float hz) const;
    float eqBandMagnitudeDbAt (int band, float hz) const;

    static juce::Colour roleColour (Role r);

private:
    void timerCallback() override;
    void pullParameters();
    void setParam (const juce::String& id, float value);
    float getParam (const juce::String& id) const;
    BalanceTargets currentTargets() const;

    juce::SharedResourcePointer<SessionBus> bus;
    int slot = -1;
    int instanceNumber = 0;
    juce::String trackName;
    std::atomic<int> currentTab { 0 };
    bool autoBalanceMode = false;

    LoudnessMeter loudness;
    SpectrumAnalyzer spectrum;
    EQEngine eq;
    Compressor comp;
    VocalSpace vocalSpace;
    juce::SmoothedValue<float> trimGain { 1.0f }, outGain { 1.0f };
    RoleGuess detected;
    std::atomic<int> detectedRoleAtomic { (int) Role::Beat };

    // cached parameter pointers
    std::atomic<float>* pRole = nullptr; std::atomic<float>* pBypass = nullptr; std::atomic<float>* pTrim = nullptr;
    std::atomic<float>* pOut = nullptr; std::atomic<float>* pAutoBal = nullptr;
    std::atomic<float>* pTargetLufs = nullptr; std::atomic<float>* pTargetTp = nullptr;
    std::atomic<float>* pEqOn = nullptr;
    struct BandPtrs { std::atomic<float>* on; std::atomic<float>* type; std::atomic<float>* freq; std::atomic<float>* gain; std::atomic<float>* q; };
    std::array<BandPtrs, kNumEQBands> pBands {};
    std::atomic<float>* pCompOn = nullptr; std::atomic<float>* pThr = nullptr; std::atomic<float>* pRatio = nullptr;
    std::atomic<float>* pAtt = nullptr; std::atomic<float>* pRel = nullptr; std::atomic<float>* pKnee = nullptr;
    std::atomic<float>* pMakeup = nullptr; std::atomic<float>* pAutoMakeup = nullptr; std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pVsOn = nullptr; std::atomic<float>* pVsAmount = nullptr; std::atomic<float>* pVsSpeed = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MUProcessor)
};
}
