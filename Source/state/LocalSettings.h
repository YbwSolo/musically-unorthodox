#pragma once
#include <juce_data_structures/juce_data_structures.h>

namespace mu
{
// Small per-computer settings file shared by every instance (update checks, lesson progress, AI tutor).
// Lives in the user's application-data folder, not in the song, so it follows the person rather than the project.
class LocalSettings
{
public:
    LocalSettings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Musically Unorthodox";
        o.filenameSuffix = ".settings";
        o.folderName = "Musically Unorthodox";
        o.osxLibrarySubFolder = "Application Support";
        o.millisecondsBeforeSaving = 800;
        file = std::make_unique<juce::PropertiesFile> (o);
    }
    juce::String get (const juce::String& key, const juce::String& fallback = {}) const { return file->getValue (key, fallback); }
    int getInt (const juce::String& key, int fallback = 0) const { return file->getIntValue (key, fallback); }
    double getDouble (const juce::String& key, double fallback = 0.0) const { return file->getDoubleValue (key, fallback); }
    void set (const juce::String& key, const juce::var& value) { file->setValue (key, value); }
    juce::File folder() const { return file->getFile().getParentDirectory(); }

private:
    std::unique_ptr<juce::PropertiesFile> file;
};
}
