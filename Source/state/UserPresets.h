#pragma once
#include <juce_core/juce_core.h>
#include "ChainPresets.h"

namespace mu
{
// A preset someone has saved themselves. It lives in a file of its own, outside any song, so it is there in every
// project and can be copied to another computer or sent to someone else.
//
// The file is JSON with the extension .mupreset:
//   { "format": "musically-unorthodox-preset", "formatVersion": 1, "pluginVersion": "0.5.12",
//     "scope": "chain" | "tune" | "reverb" | ...,  "name": "...", "note": "...", "saved": "2026-11-14T12:00:00Z",
//     "level": -21.5,        (whole-chain presets only: the loudness of the track it was saved on)
//     "values": { "<parameter id>": <value in the parameter's own units>, ... } }
// "scope" says what the preset covers: "chain" is the whole plugin, anything else is one tool. Values are stored in
// real units (dB, ms, Hz, %), not as positions of a knob, so a file keeps its meaning if a control's range changes.
// A file from a newer format than this build understands is refused; parameters this build does not have are
// skipped and counted, and every value is brought inside its control's range when it is loaded.
struct UserPreset
{
    juce::String scope, name, note, pluginVersion;
    std::vector<ParamValue> values;
    juce::File file;
    int skipped = 0;                    // values in the file that this build has no control for
    float level = -1000.0f;             // chain presets: how loud the track was when it was saved (LUFS), so thresholds can follow a different track; -1000 = not known
};

class UserPresets
{
public:
    static constexpr int kFormatVersion = 1;
    static constexpr const char* kFormatName = "musically-unorthodox-preset";
    static constexpr const char* kExtension = ".mupreset";

    // Where they are kept: <application data>/Musically Unorthodox/Presets/<scope>/<name>.mupreset
    static juce::File folder();
    static void setFolderForTesting (const juce::File& f);

    static std::vector<UserPreset> list (const juce::String& scope);            // sorted by name; unreadable files are left out
    static bool find (const juce::String& scope, const juce::String& name, UserPreset& out);
    // Writes to a temporary file beside the target and swaps it in, so a crash or a full disk never leaves half a preset.
    static juce::Result save (UserPreset& preset, bool replaceExisting);
    static juce::Result read (const juce::File& file, UserPreset& out);
    static juce::Result remove (const UserPreset& preset);
    static juce::Result rename (UserPreset& preset, const juce::String& newName);
    // Import: checks the file, then copies it into the folder (under a free name if that one is taken).
    static juce::Result importFile (const juce::File& source, UserPreset& out);
    static juce::Result exportTo (const UserPreset& preset, const juce::File& target);

    static juce::String cleanName (const juce::String& typed);                  // what is left of a typed name once it is safe as a file name
    static juce::String toJson (const UserPreset& preset);
    static juce::Result fromJson (const juce::String& json, UserPreset& out);
};
}
