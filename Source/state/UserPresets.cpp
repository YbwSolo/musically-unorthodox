#include "UserPresets.h"
#include <algorithm>
#include <cmath>

namespace mu
{
namespace
{
juce::File& testFolder() { static juce::File f; return f; }

juce::File scopeFolder (const juce::String& scope) { return UserPresets::folder().getChildFile (scope.retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789-")); }
juce::File fileFor (const juce::String& scope, const juce::String& name) { return scopeFolder (scope).getChildFile (UserPresets::cleanName (name) + UserPresets::kExtension); }
}

juce::File UserPresets::folder()
{
    if (testFolder() != juce::File()) return testFolder();
    const auto env = juce::SystemStats::getEnvironmentVariable ("MU_PRESET_DIR", {});
    if (env.isNotEmpty()) return juce::File (env);
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    return base.getChildFile ("Musically Unorthodox").getChildFile ("Presets");
}

void UserPresets::setFolderForTesting (const juce::File& f) { testFolder() = f; }

juce::String UserPresets::cleanName (const juce::String& typed)
{
    auto n = typed.trim().removeCharacters ("\\/:*?\"<>|\r\n\t").substring (0, 60).trim();
    while (n.startsWith (".")) n = n.substring (1).trim();
    return n;
}

juce::String UserPresets::toJson (const UserPreset& p)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format", kFormatName);
    root->setProperty ("formatVersion", kFormatVersion);
    root->setProperty ("pluginVersion", p.pluginVersion);
    root->setProperty ("scope", p.scope);
    root->setProperty ("name", p.name);
    root->setProperty ("note", p.note);
    root->setProperty ("saved", juce::Time::getCurrentTime().toISO8601 (true));
    if (p.level > -200.0f) root->setProperty ("level", (double) p.level);
    auto* values = new juce::DynamicObject();
    for (auto& v : p.values) values->setProperty (v.id, (double) v.value);
    root->setProperty ("values", juce::var (values));
    return juce::JSON::toString (juce::var (root));
}

juce::Result UserPresets::fromJson (const juce::String& json, UserPreset& out)
{
    juce::var parsed;
    if (json.length() > 400000 || juce::JSON::parse (json, parsed).failed() || ! parsed.isObject())
        return juce::Result::fail ("That file is not a preset: it could not be read.");
    if (parsed.getProperty ("format", {}).toString() != kFormatName)
        return juce::Result::fail ("That file is not a Musically Unorthodox preset.");
    const int version = (int) parsed.getProperty ("formatVersion", 0);
    if (version < 1) return juce::Result::fail ("That preset file is damaged: it does not say which format it is in.");
    if (version > kFormatVersion) return juce::Result::fail ("That preset was saved by a newer version of the plugin. Update the plugin (Settings) and load it again.");
    out.scope = parsed.getProperty ("scope", {}).toString();
    out.name = cleanName (parsed.getProperty ("name", {}).toString());
    out.note = parsed.getProperty ("note", {}).toString().substring (0, 400);
    out.pluginVersion = parsed.getProperty ("pluginVersion", {}).toString();
    const auto level = parsed.getProperty ("level", {});
    out.level = (level.isDouble() || level.isInt()) && std::isfinite ((double) level) ? juce::jlimit (-100.0f, 20.0f, (float) (double) level) : -1000.0f;
    if (out.scope.isEmpty() || out.name.isEmpty()) return juce::Result::fail ("That preset file is damaged: it has no name, or does not say what it is for.");
    const auto values = parsed.getProperty ("values", {});
    auto* object = values.getDynamicObject();
    if (object == nullptr || object->getProperties().size() == 0) return juce::Result::fail ("That preset file is damaged: it holds no settings.");
    out.values.clear();
    for (auto& item : object->getProperties())
    {
        if (! (item.value.isDouble() || item.value.isInt() || item.value.isInt64() || item.value.isBool())) continue;
        const double v = (double) item.value;
        if (std::isfinite (v)) out.values.push_back ({ item.name.toString(), (float) v });
    }
    if (out.values.empty()) return juce::Result::fail ("That preset file is damaged: it holds no settings.");
    return juce::Result::ok();
}

juce::Result UserPresets::read (const juce::File& file, UserPreset& out)
{
    if (! file.existsAsFile()) return juce::Result::fail ("That preset file is no longer there.");
    if (file.getSize() > 400000) return juce::Result::fail ("That file is far too large to be a preset.");
    const auto result = fromJson (file.loadFileAsString(), out);
    if (result.wasOk()) out.file = file;
    return result;
}

std::vector<UserPreset> UserPresets::list (const juce::String& scope)
{
    std::vector<UserPreset> out;
    for (auto& f : scopeFolder (scope).findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension))
    {
        UserPreset p;
        if (read (f, p).wasOk() && p.scope == scope) out.push_back (std::move (p));
    }
    std::sort (out.begin(), out.end(), [] (const UserPreset& a, const UserPreset& b) { return a.name.compareIgnoreCase (b.name) < 0; });
    return out;
}

bool UserPresets::find (const juce::String& scope, const juce::String& name, UserPreset& out)
{
    for (auto& p : list (scope)) if (p.name == name) { out = p; return true; }
    return false;
}

juce::Result UserPresets::save (UserPreset& p, bool replaceExisting)
{
    p.name = cleanName (p.name);
    if (p.name.isEmpty()) return juce::Result::fail ("Give the preset a name first.");
    if (p.scope.isEmpty() || p.values.empty()) return juce::Result::fail ("There is nothing to save.");
    const auto target = fileFor (p.scope, p.name);
    if (target.existsAsFile() && ! replaceExisting) return juce::Result::fail ("You already have a preset called \"" + p.name + "\". Choose another name, or use Save to replace it.");
    if (! target.getParentDirectory().createDirectory()) return juce::Result::fail ("The presets folder could not be created: " + target.getParentDirectory().getFullPathName());
    juce::TemporaryFile temp (target);
    if (! temp.getFile().replaceWithText (toJson (p)) || ! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("The preset could not be written. Check that the disk is not full: " + target.getFullPathName());
    p.file = target;
    return juce::Result::ok();
}

juce::Result UserPresets::remove (const UserPreset& p)
{
    if (! p.file.existsAsFile()) return juce::Result::fail ("That preset is already gone.");
    if (! p.file.isAChildOf (folder())) return juce::Result::fail ("Only presets in your presets folder can be deleted from here.");
    // to the bin where there is one, so a slip can be undone
    if (! p.file.moveToTrash() && ! p.file.deleteFile()) return juce::Result::fail ("The preset could not be deleted.");
    return juce::Result::ok();
}

juce::Result UserPresets::rename (UserPreset& p, const juce::String& newName)
{
    const auto name = cleanName (newName);
    if (name.isEmpty()) return juce::Result::fail ("Give the preset a name first.");
    if (name == p.name) return juce::Result::ok();
    if (fileFor (p.scope, name).existsAsFile() && ! name.equalsIgnoreCase (p.name)) return juce::Result::fail ("You already have a preset called \"" + name + "\".");
    const auto old = p.file;
    UserPreset renamed = p;
    renamed.name = name;
    const auto saved = save (renamed, true);
    if (saved.failed()) return saved;
    if (old != renamed.file) old.deleteFile();
    p = renamed;
    return juce::Result::ok();
}

juce::Result UserPresets::importFile (const juce::File& source, UserPreset& out)
{
    UserPreset p;
    const auto ok = read (source, p);
    if (ok.failed()) return ok;
    // keep what is already there: an imported preset with a name in use gets a number after it
    const auto base = p.name;
    for (int n = 2; fileFor (p.scope, p.name).existsAsFile() && n < 100; ++n) p.name = cleanName (base.substring (0, 54) + " " + juce::String (n));
    const auto saved = save (p, false);
    if (saved.wasOk()) out = p;
    return saved;
}

juce::Result UserPresets::exportTo (const UserPreset& p, const juce::File& target)
{
    auto file = target.hasFileExtension (kExtension) ? target : target.withFileExtension (kExtension);
    juce::TemporaryFile temp (file);
    if (! temp.getFile().replaceWithText (toJson (p)) || ! temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail ("The preset could not be written to " + file.getFullPathName());
    return juce::Result::ok();
}
}
