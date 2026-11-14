#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "BalanceEngine.h"
#include "../dsp/KeyFinder.h"

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
    virtual void setAcceptsBalance (bool on) = 0;
    virtual bool fillBandsDb (std::array<float, kAnalysisBands>& out) const = 0;
    virtual void applyMaskingCut (float hz, float db) = 0;
    virtual float cutDbNear (float hz) const = 0;      // the deepest EQ cut this track already has around hz (0 if none)
    virtual void setSessionInstanceNumber (int n) = 0;
    virtual void resetMeasurement() = 0;

    // song key
    virtual KeyEstimate getMusicKey() const = 0;                    // from the chords and bass heard on this track
    virtual bool getNameKey (int& key, bool& minor) const = 0;     // a key written in the track's name
    virtual void setSongKey (int key, bool minor) = 0;              // sets this track's Tune key and scale
    virtual bool isTuneOn() const = 0;

    // peaks
    virtual float getPeakOverDb() const = 0;        // how far this track's loudest moments reach past -1 dB (0 = they do not)
    virtual bool isCompressorOn() const = 0;
    virtual juce::String tamePeaks() = 0;           // bring the loud moments down; returns what was done
    virtual void turnOutputDown (float db) = 0;     // lower what leaves the track, after all its processing
    virtual float getStereoCorrelation() const = 0;
};

// The key of the song as far as the session can tell, and where that came from.
struct SongKey
{
    bool found = false;
    int key = 0;
    bool minor = false;
    float confidence = 0.0f;
    bool fromName = false;
    juce::String source;            // the track it was read or heard from
    juce::String note;              // set when the name and the sound disagree
    juce::String text() const { static const char* const n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return juce::String (n[juce::jlimit (0, 11, key)]) + (minor ? " minor" : " major"); }
};

// A track whose loud moments are out of proportion to the rest of it.
struct HotTrack
{
    int slot = -1;
    juce::String name;
    Role role = Role::Beat;
    float overDb = 0.0f;            // how far past -1 dB its peaks reach
    float crestDb = 0.0f;           // peak above average loudness
    bool compressorOn = false;
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
    void setTrackLocked (int slot, bool locked);

    // Song key: read from a track name when one states it, otherwise heard from the master, the beat or an instrument.
    SongKey findSongKey() const;
    int sendKeyToAll (int key, bool minor);                // every instance takes the key; returns how many have Tune on
    // Tracks whose peaks are too hot, worst first, and the fix for one of them.
    std::vector<HotTrack> findHotPeaks() const;
    juce::String tamePeaks (int slot);
    juce::String takeNotice();                             // something the session did by itself since the last call, to show the user
    float getMasterCorrelation() const;                    // 1 when there is no master

    void setAutoBalance (bool on, BalanceTargets targets);
    bool isAutoBalance() const { return autoBalance; }

    // Low latency mode, for recording through the plugin: every instance in the session drops the stages that
    // delay the sound. Not saved with the song, so a session always opens in full quality.
    void setLowLatency (bool on) noexcept { lowLatency.store (on); }
    bool isLowLatency() const noexcept { return lowLatency.load (std::memory_order_relaxed); }

    // "Learn & balance": wipe every instance's integrated reading, listen for a while, then apply the plan.
    void startLearn (float seconds, BalanceTargets targets);
    void cancelLearn();
    LearnState getLearnState() const;
    void resetAllMeasurements();
    BalanceTargets getTargets() const { return targets; }
    BalancePlan getLastPlan() const;
    std::array<float, kAnalysisBands> bandCentres() const;

    // Reference tone, shared by every instance: capture it on a track playing a finished song,
    // then match it from the master or any other track.
    void setReference (const std::array<float, kAnalysisBands>& bandsDb, const juce::String& sourceName);
    bool getReference (std::array<float, kAnalysisBands>& out, juce::String& sourceName) const;
    void clearReference();

    std::function<void()> onSessionChanged;   // UI hook (message thread)

private:
    void timerCallback() override;

    std::array<Slot, kMaxInstances> slots;
    mutable juce::CriticalSection lock;
    bool autoBalance = false;
    std::atomic<bool> lowLatency { false };
    BalanceTargets targets;
    BalancePlan lastPlan;
    int nextInstanceNumber = 1;
    bool learning = false;
    double learnStartMs = 0.0, learnLengthMs = 0.0;
    juce::String learnMessage;
    struct FollowUp { int slot = -1; double dueMs = 0.0; int round = 0; };
    std::vector<FollowUp> followUps;       // tracks whose peaks were tamed: look again once the result has been heard
    juce::String notice;
    double rebalanceAtMs = 0.0;     // after peaks were tamed during a learn: balance once more when the new levels have been heard
    double settleUntilMs = 0.0, lastAutoApplyMs = 0.0, resetAtMs = 0.0;     // after a balance, wait for fresh measurements before judging again
    bool hasReference = false;
    std::array<float, kAnalysisBands> referenceBands {};
    juce::String referenceName;
};
}
