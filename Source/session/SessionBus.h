#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "BalanceEngine.h"

namespace mu
{
// What one plugin instance exposes to the session coordinator. All calls happen on the message thread
// except publish(), which the audio thread uses through the slot atomics.
class SessionClient
{
public:
    virtual ~SessionClient() = default;
    virtual juce::String getDisplayName() const = 0;
    virtual Role getEffectiveRole() const = 0;
    virtual float getTrimDb() const = 0;
    virtual void setTrimDb (float db) = 0;
    virtual bool acceptsBalance() const = 0;
    virtual bool fillBandsDb (std::array<float, kAnalysisBands>& out) const = 0;
    virtual void applyMaskingCut (float hz, float db) = 0;
    virtual void setSessionInstanceNumber (int n) = 0;
    virtual void resetMeasurement() = 0;
};

struct LearnState
{
    bool active = false;
    float progress = 0.0f;        // 0..1
    juce::String message;
};

// One process-wide bus shared by every instance in the host (via juce::SharedResourcePointer).
// Instances publish metering into lock-free slots; the master instance's coordinator reads them.
class SessionBus : private juce::Timer
{
public:
    static constexpr int kMaxInstances = 64;

    struct Slot
    {
        std::atomic<bool> used { false };
        std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f };
        std::atomic<float> truePeak { -100.0f }, crest { 0.0f };
        std::atomic<bool> active { false };
        std::atomic<int> role { 0 };
        SessionClient* client = nullptr;   // message thread only
    };

    SessionBus();
    ~SessionBus() override;

    int registerClient (SessionClient* c);         // returns slot
    void unregisterClient (int slot);

    // Audio thread
    void publish (int slot, float momentary, float shortTerm, float integrated, float truePeak, float crest, bool active, Role role) noexcept;
    float getVocalKeyLevel() const noexcept;        // 0..1, how present the lead vocal is right now
    const Slot& getSlot (int i) const { return slots[(size_t) i]; }

    // Message thread
    std::vector<TrackInfo> collectTracks() const;
    int getMasterSlot() const;
    int getNumInstances() const;
    BalancePlan computePlan (const BalanceTargets& targets) const;
    void applyPlan (const BalancePlan& plan, float fraction = 1.0f);
    std::vector<MaskingClash> computeMasking() const;
    void applyMaskingClash (const MaskingClash& c);

    void setAutoBalance (bool on, BalanceTargets targets);
    bool isAutoBalance() const { return autoBalance; }

    // "Learn & balance": wipe every instance's integrated reading, listen for a while, then apply the plan.
    void startLearn (float seconds, BalanceTargets targets);
    void cancelLearn();
    LearnState getLearnState() const;
    void resetAllMeasurements();
    BalanceTargets getTargets() const { return targets; }
    BalancePlan getLastPlan() const;
    std::array<float, kAnalysisBands> bandCentres() const;

    std::function<void()> onSessionChanged;   // UI hook (message thread)

private:
    void timerCallback() override;

    std::array<Slot, kMaxInstances> slots;
    mutable juce::CriticalSection lock;
    bool autoBalance = false;
    BalanceTargets targets;
    BalancePlan lastPlan;
    int nextInstanceNumber = 1;
    bool learning = false;
    double learnStartMs = 0.0, learnLengthMs = 0.0;
    juce::String learnMessage;
};
}
