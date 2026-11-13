#pragma once
#include <juce_core/juce_core.h>
#include <atomic>

namespace mu
{
// Offline licence check. A Pro key is "MU-XXXX-XXXX-XXXX-XXXX" derived from the buyer's email.
// Generate keys with scripts/make_key.py (same secret). This is a simple scheme meant to be
// swapped for a server check later; keep the secret in sync in both places.
class License
{
public:
    static License& get();

    bool isPro() const { return pro.load(); }
    juce::String getEmail() const;
    juce::String getKey() const;

    // Returns true and stores the licence when the key matches the email.
    bool activate (const juce::String& email, const juce::String& key);
    void deactivate();

    static juce::String makeKeyForEmail (const juce::String& email);
    static bool isFeatureFree (const juce::String& featureId);

private:
    License();
    void load();
    void save();
    juce::File getLicenseFile() const;

    std::atomic<bool> pro { false };
    juce::String email, key;
    mutable juce::CriticalSection lock;
};

// Feature gates used by the UI. Everything that *analyses* is free; everything that *fixes for you* is Pro.
namespace features
{
    inline const char* sessionBalance = "balance";     // free
    inline const char* levelsPage     = "levels";      // free
    inline const char* manualEQ       = "eq";          // free
    inline const char* manualComp     = "comp";        // free
    inline const char* autoBalance    = "autobalance"; // pro (continuous)
    inline const char* cleanHarsh     = "cleanharsh";  // pro
    inline const char* vocalSpace     = "vocalspace";  // pro
    inline const char* autoComp       = "autocomp";    // pro
    inline const char* maskingFix     = "masking";     // pro
}
}
