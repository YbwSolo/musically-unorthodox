#include "Updater.h"
#include <juce_cryptography/juce_cryptography.h>
#if ! JUCE_WINDOWS
 #include <unistd.h>
#endif

namespace mu
{
static const char* const kBundleName = "Musically Unorthodox";

Updater::Updater() : juce::Thread ("MU updater"), baseUrl (MU_UPDATE_BASE_URL) {}
Updater::~Updater() { stopThread (4000); }

int Updater::currentBuild() { return MU_BUILD_NUMBER; }
juce::String Updater::currentVersion() { return MU_VERSION_STRING; }

UpdateState Updater::getState() const
{
    const juce::ScopedLock sl (lock);
    return state;
}

void Updater::set (UpdateState::Phase p, const juce::String& message, float progress)
{
    const juce::ScopedLock sl (lock);
    state.phase = p; state.message = message; state.progress = progress;
}

void Updater::configureForTesting (const juce::String& feed, const juce::File& au, const juce::File& vst3, int pretendBuild)
{
    baseUrl = feed; testAU = au; testVST3 = vst3; buildOverride = pretendBuild;
}

void Updater::checkNow()
{
    if (isThreadRunning()) return;
    job.store ((int) Job::Check);
    set (UpdateState::Phase::Checking, "Checking for updates...");
    startThread();
}

void Updater::autoCheck()
{
    if (currentBuild() <= 0 && buildOverride < 0) return;           // a local development build never nags
    const auto phase = getState().phase;
    if (phase != UpdateState::Phase::Idle) return;
    const double last = settings->getDouble ("lastUpdateCheckMs", 0.0);
    const double now = (double) juce::Time::currentTimeMillis();
    if (now - last < 6.0 * 3600.0 * 1000.0) return;
    checkNow();
}

void Updater::installNow()
{
    if (isThreadRunning() || getState().phase != UpdateState::Phase::Available) return;
    job.store ((int) Job::Install);
    set (UpdateState::Phase::Downloading, "Downloading the update...");
    startThread();
}

void Updater::run()
{
    const auto j = (Job) job.exchange ((int) Job::None);
    if (j == Job::Check) doCheck();
    else if (j == Job::Install) doInstall();
}

bool Updater::parseFeed (const juce::String& json, UpdateInfo& out)
{
    const auto v = juce::JSON::parse (json);
    if (! v.isObject()) return false;
    out.build = (int) v.getProperty ("build", 0);
    out.version = v.getProperty ("version", "").toString();
    out.file = v.getProperty ("file", "").toString();
    out.sha256 = v.getProperty ("sha256", "").toString().toLowerCase();
    out.notes = v.getProperty ("notes", "").toString();
    // the file name is joined onto our own base address, so it must be a plain name, never a path or another site
    const bool safeName = out.file.isNotEmpty() && out.file.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") && ! out.file.startsWith (".");
    return out.build > 0 && safeName && out.sha256.length() == 64;
}

void Updater::doCheck()
{
    int status = 0;
    juce::String text;
    {
        juce::URL url (baseUrl + "/latest.json");
        auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                 .withConnectionTimeoutMs (10000).withNumRedirectsToFollow (8).withStatusCode (&status));
        if (stream != nullptr) text = stream->readEntireStreamAsString();
    }
    settings->set ("lastUpdateCheckMs", (double) juce::Time::currentTimeMillis());

    UpdateInfo info;
    if (text.isEmpty() || ! parseFeed (text, info))
    {
        set (UpdateState::Phase::Failed, status == 404 ? "No update feed was found. The download page may be private."
                                                       : "Could not reach the update server. Check the internet connection and try again.");
        return;
    }
    const int mine = buildOverride >= 0 ? buildOverride : currentBuild();
    {
        const juce::ScopedLock sl (lock);
        state.latest = info;
    }
    if (info.build > mine) set (UpdateState::Phase::Available, "Version " + info.version + " is available.");
    else                   set (UpdateState::Phase::UpToDate, "This is the newest version.");
}

bool Updater::download (const juce::String& address, const juce::File& target, juce::String& error)
{
    int status = 0;
    juce::URL url (address);
    auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                         .withConnectionTimeoutMs (15000).withNumRedirectsToFollow (8).withStatusCode (&status));
    if (in == nullptr || (status != 0 && status >= 400)) { error = "The update could not be downloaded."; return false; }
    target.deleteFile();
    juce::FileOutputStream out (target);
    if (! out.openedOk()) { error = "No space to save the download."; return false; }
    const auto total = in->getTotalLength();
    juce::HeapBlock<char> buffer (1 << 16);
    juce::int64 done = 0;
    while (! in->isExhausted())
    {
        if (threadShouldExit()) { error = "Cancelled."; return false; }
        const int n = in->read (buffer, 1 << 16);
        if (n <= 0) break;
        if (! out.write (buffer, (size_t) n)) { error = "The download could not be saved."; return false; }
        done += n;
        set (UpdateState::Phase::Downloading, "Downloading the update...", total > 0 ? (float) ((double) done / (double) total) : 0.0f);
    }
    out.flush();
    return done > 0;
}

juce::String Updater::sha256Of (const juce::File& f)
{
    juce::FileInputStream in (f);
    if (! in.openedOk()) return {};
    return juce::SHA256 (in).toHexString().toLowerCase();
}

// Copies every file of the new bundle next to its old counterpart and renames it into place. A plugin that
// is loaded keeps using the old file it already has open, so this is safe while the host is running.
bool Updater::replaceTree (const juce::File& source, const juce::File& destination, juce::String& error)
{
    if (! source.isDirectory()) { error = "The update is incomplete."; return false; }
    if (! destination.createDirectory()) { error = "No permission to write to " + destination.getFullPathName(); return false; }

    juce::StringArray kept;
    for (const auto& entry : juce::RangedDirectoryIterator (source, true, "*", juce::File::findFilesAndDirectories))
    {
        const auto from = entry.getFile();
        const auto rel = from.getRelativePathFrom (source);
        const auto to = destination.getChildFile (rel);
        if (from.isDirectory()) { if (! to.createDirectory()) { error = "No permission to write to " + to.getFullPathName(); return false; } continue; }
        kept.add (rel);
        const auto staged = to.getSiblingFile (to.getFileName() + ".mu-new");
        staged.deleteFile();
        if (! from.copyFileTo (staged)) { error = "No permission to write to " + to.getParentDirectory().getFullPathName(); return false; }
       #if ! JUCE_WINDOWS
        if (::access (from.getFullPathName().toRawUTF8(), X_OK) == 0) staged.setExecutePermission (true);
       #endif
        if (! staged.moveFileTo (to)) { staged.deleteFile(); error = "Could not replace " + to.getFileName(); return false; }
    }
    // anything the new version no longer ships
    for (const auto& entry : juce::RangedDirectoryIterator (destination, true, "*", juce::File::findFiles))
        if (! kept.contains (entry.getFile().getRelativePathFrom (destination))) entry.getFile().deleteFile();
    return true;
}

juce::File Updater::destinationFor (const juce::String& folder, const juce::String& bundle, const juce::File& testOverride) const
{
    if (testOverride != juce::File()) return testOverride;
    auto writable = [] (const juce::File& b) { return b.isDirectory() && b.getChildFile ("Contents").hasWriteAccess() && b.getChildFile ("Contents/MacOS").hasWriteAccess(); };

    // 1. the copy that is running right now, if it is this kind of bundle and we may write to it
    auto running = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getParentDirectory().getParentDirectory();
    if (running.getFileName() == bundle && writable (running)) return running;
    // 2. the shared location the installer uses, 3. the personal one
    const juce::File shared ("/Library/Audio/Plug-Ins/" + folder + "/" + bundle);
    const auto personal = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Plug-Ins/" + folder + "/" + bundle);
    if (writable (shared)) return shared;
    if (personal.isDirectory() || shared.isDirectory()) return personal;
    return {};        // this format is not installed on this computer, so leave it alone
}

void Updater::doInstall()
{
    const auto info = getState().latest;
    auto work = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mu-update-" + juce::String (info.build));
    work.deleteRecursively();
    work.createDirectory();
    const auto zip = work.getChildFile ("bundles.zip");
    juce::String error;

    if (! download (baseUrl + "/" + info.file, zip, error)) { set (UpdateState::Phase::Failed, error); return; }
    if (sha256Of (zip) != info.sha256) { set (UpdateState::Phase::Failed, "The download was damaged on the way. Try again."); work.deleteRecursively(); return; }

    set (UpdateState::Phase::Installing, "Installing...", 1.0f);
    const auto unpacked = work.getChildFile ("unpacked");
    unpacked.createDirectory();
    juce::ChildProcess unzip;
   #if JUCE_MAC
    const juce::StringArray command { "/usr/bin/ditto", "-x", "-k", zip.getFullPathName(), unpacked.getFullPathName() };
   #else
    const juce::StringArray command { "unzip", "-q", "-o", zip.getFullPathName(), "-d", unpacked.getFullPathName() };
   #endif
    if (! unzip.start (command) || ! unzip.waitForProcessToFinish (120000) || unzip.getExitCode() != 0)
    { set (UpdateState::Phase::Failed, "The update could not be unpacked."); work.deleteRecursively(); return; }

    struct Format { const char* folder; const char* extension; juce::File test; };
    const Format formats[] = { { "Components", ".component", testAU }, { "VST3", ".vst3", testVST3 } };
    int installed = 0;
    for (auto& f : formats)
    {
        const auto bundle = juce::String (kBundleName) + f.extension;
        const auto source = unpacked.getChildFile (f.folder).getChildFile (bundle);
        if (! source.isDirectory()) continue;
        const auto dest = destinationFor (f.folder, bundle, f.test);
        if (dest == juce::File()) continue;
        if (! replaceTree (source, dest, error)) { set (UpdateState::Phase::Failed, error + ". Use the installer from the download page instead."); work.deleteRecursively(); return; }
        ++installed;
    }
    work.deleteRecursively();
    if (installed == 0) { set (UpdateState::Phase::Failed, "Could not find where the plugin is installed. Use the installer from the download page instead."); return; }
    set (UpdateState::Phase::Installed, "Version " + info.version + " is installed. Quit and reopen your music software to start using it.", 1.0f);
}
}
