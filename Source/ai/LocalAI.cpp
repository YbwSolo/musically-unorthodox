#include "LocalAI.h"
#include <juce_cryptography/juce_cryptography.h>
#if JUCE_MAC
 #include <sys/sysctl.h>
#endif

namespace mu
{
namespace
{
const int kFirstPort = 47611;               // the helper only ever listens on this computer
const char* const kAlias = "mu-tutor";      // lets us recognise our own helper if it is already running
const char* const kPlainName = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-";

juce::String archFolder()
{
   #if JUCE_MAC
    int arm = 0; size_t size = sizeof (arm);
    if (sysctlbyname ("hw.optional.arm64", &arm, &size, nullptr, 0) == 0 && arm == 1) return "arm64";
   #endif
    return "x64";
}

juce::String megabytes (juce::int64 bytes)
{
    const double mb = (double) bytes / 1.0e6;
    return mb >= 1000.0 ? juce::String (mb / 1000.0, 1) + " GB" : juce::String (juce::roundToInt (mb)) + " MB";
}

juce::var message (const char* role, const juce::String& content)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("role", role); o->setProperty ("content", content);
    return juce::var (o);
}

int httpStatus (const juce::String& address, int timeoutMs, juce::String* body = nullptr)
{
    int code = 0;
    auto in = juce::URL (address).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                         .withConnectionTimeoutMs (timeoutMs).withStatusCode (&code));
    if (in == nullptr) return 0;
    if (body != nullptr) *body = in->readEntireStreamAsString();
    return code;
}
}

LocalAI::LocalAI() : juce::Thread ("MU tutor"), feedBase (MU_UPDATE_BASE_URL)
{
    extraArgs = juce::SystemStats::getEnvironmentVariable ("MU_TUTOR_ARGS", {});
    const auto dir = juce::SystemStats::getEnvironmentVariable ("MU_TUTOR_DIR", {});
    if (dir.isNotEmpty()) { testFolder = juce::File (dir); testing = true; }
    const auto feed = juce::SystemStats::getEnvironmentVariable ("MU_TUTOR_FEED", {});
    if (feed.isNotEmpty()) { feedBase = feed; testing = true; }
   #if ! JUCE_MAC
    if (! testing) status.setup = Setup::Unsupported;
   #endif
}

LocalAI::~LocalAI()
{
    stopThread (5000);
    stopServer();
}

void LocalAI::configureForTesting (const juce::String& feed, const juce::File& folder, const juce::String& extra)
{
    feedBase = feed; testFolder = folder; extraArgs = extra; testing = true;
    const juce::ScopedLock sl (lock);
    if (status.setup == Setup::Unsupported) status.setup = Setup::NotInstalled;
}

LocalAI::Status LocalAI::getStatus() const
{
    const juce::ScopedLock sl (lock);
    return status;
}

void LocalAI::setSetup (Setup s, const juce::String& text, float progress)
{
    const juce::ScopedLock sl (lock);
    status.setup = s; status.setupMessage = text; status.progress = progress;
}

void LocalAI::refresh()
{
    if (isThreadRunning()) return;
    job.store (Detect);
    startThread();
}

void LocalAI::forgetConversation()
{
    const juce::ScopedLock sl (lock);
    history.clear();
}

void LocalAI::ask (const juce::String& systemPrompt, const juce::String& question, int conversation)
{
    if (isThreadRunning()) return;
    {
        const juce::ScopedLock sl (lock);
        const auto now = juce::Time::currentTimeMillis();
        if (conversation != historyOwner || now - historyTime > 10 * 60 * 1000) history.clear();
        historyOwner = conversation; historyTime = now;
        pendingSystem = systemPrompt; pendingQuestion = question;
        status.busy = true; status.error.clear();
    }
    job.store (Ask);
    startThread();
}

void LocalAI::installBuiltIn()
{
    if (isThreadRunning() || getStatus().setup == Setup::Unsupported) return;
    setSetup (Setup::Downloading, "Getting ready...");
    job.store (Install);
    startThread();
}

// The model is chosen first, then downloaded. If the tutor is in the middle of looking for what is installed,
// the download starts as soon as that has finished.
void LocalAI::installModel (const juce::String& id)
{
    if (getStatus().setup == Setup::Unsupported) return;
    if (id.isNotEmpty()) settings->set ("tutorModel", id);
    lastUpgradeCheck = 0;
    setSetup (Setup::Downloading, "Getting ready...");
    if (isThreadRunning()) { installNext.store (true); return; }
    job.store (Install);
    startThread();
}

void LocalAI::cancelInstall()
{
    if (getStatus().setup == Setup::Downloading) signalThreadShouldExit();
}

void LocalAI::removeBuiltIn()
{
    if (isThreadRunning()) return;
    stopServer();
    for (auto& path : candidateFolders())
    {
        const juce::File folder (path);
        folder.getChildFile ("helper").deleteRecursively();
        folder.getChildFile ("tutor.json").deleteFile();
        for (const auto& e : juce::RangedDirectoryIterator (folder, false, "model-*", juce::File::findFiles)) e.getFile().deleteFile();
    }
    setSetup (Setup::NotInstalled, {});
    refresh();
}

void LocalAI::run()
{
    switch (job.exchange (None))
    {
        case Detect:  detect(); break;
        case Ask:     doAsk(); break;
        case Install: doInstall(); break;
        default: break;
    }
    if (installNext.exchange (false) && ! threadShouldExit()) doInstall();
}

//==================================================================== where the tutor lives
juce::StringArray LocalAI::candidateFolders() const
{
    juce::StringArray folders;
    if (testFolder != juce::File()) { folders.add (testFolder.getFullPathName()); return folders; }
   #if JUCE_MAC
    folders.add ("/Users/Shared/Musically Unorthodox");          // one copy for every account on a studio Mac
   #endif
    folders.add (settings->folder().getChildFile ("Tutor").getFullPathName());
    return folders;
}

juce::File LocalAI::installFolder() const
{
    for (auto& path : candidateFolders())
    {
        const juce::File folder (path);
        if ((folder.isDirectory() || folder.createDirectory()) && folder.hasWriteAccess()) return folder;
    }
    return {};
}

bool LocalAI::findInstalled (Installed& out) const
{
    for (auto& path : candidateFolders())
    {
        const juce::File folder (path);
        const auto manifest = juce::JSON::parse (folder.getChildFile ("tutor.json").loadFileAsString());
        if (! manifest.isObject()) continue;
        const auto h = folder.getChildFile ("helper").getChildFile (archFolder()).getChildFile ("llama-server");
        const auto m = folder.getChildFile (manifest.getProperty ("model", "").toString());
        if (! h.existsAsFile() || ! m.existsAsFile() || m.getSize() != (juce::int64) manifest.getProperty ("modelBytes", 0)) continue;
        out.helper = h; out.model = m;
        out.modelName = manifest.getProperty ("modelName", "built-in model").toString();
        out.version = manifest.getProperty ("version", "").toString();
        return true;
    }
    return false;
}

// Once in a while, read the release feed: which model this computer would get, whether there is another to
// choose, and whether what is installed (if anything) is different from what is on offer now.
void LocalAI::readOffer (const Installed* have)
{
    const auto now = juce::Time::currentTimeMillis();
    if (lastUpgradeCheck != 0 && now - lastUpgradeCheck < (juce::int64) 6 * 3600 * 1000) return;
    lastUpgradeCheck = now;
    juce::String feed;
    httpStatus (feedBase + "/latest.json", 8000, &feed);
    TutorPack pack;
    if (! parseTutorFeed (feed, pack, testing)) return;
    choose (pack);
    const bool newModel = have != nullptr && have->model.getFileName() != "model-" + pack.modelSha256.substring (0, 12) + ".gguf";
    const bool newHelper = have != nullptr && have->version != pack.version;
    const juce::ScopedLock sl (lock);
    status.upgradeAvailable = newModel || newHelper;
    status.upgradeName = pack.modelName;
    status.upgradeBytes = newModel ? pack.modelBytes : 0;
    status.offerName = pack.modelName; status.offerBytes = pack.modelBytes;
    status.otherId.clear(); status.otherName.clear();
    for (auto& m : pack.models)
        if (m.sha256 != pack.modelSha256) { status.otherId = m.id; status.otherName = m.name; }
    const int memoryGb = juce::roundToInt ((double) juce::SystemStats::getMemorySizeInMegabytes() / 1024.0);
    status.choices.clear();
    for (auto& m : pack.models)
    {
        Status::Choice c;
        c.id = m.id; c.name = m.name; c.bytes = m.bytes; c.minMemoryGb = m.minMemoryGb; c.appleSilicon = m.appleSilicon;
        c.fits = memoryGb >= m.minMemoryGb && (archFolder() == "arm64" || ! m.appleSilicon);
        c.chosen = m.sha256 == pack.modelSha256;
        status.choices.push_back (c);
    }
}

//==================================================================== feed
bool LocalAI::parseTutorFeed (const juce::String& json, TutorPack& out, bool anyModelHost)
{
    const auto t = juce::JSON::parse (json).getProperty ("tutor", juce::var());
    if (! t.isObject()) return false;
    out.version = t.getProperty ("version", "").toString();
    out.file = t.getProperty ("file", "").toString();
    out.sha256 = t.getProperty ("sha256", "").toString().toLowerCase();
    out.models.clear();
    auto read = [&] (const juce::var& m)
    {
        TutorModel model;
        model.id = m.getProperty ("id", "light").toString();
        model.name = m.getProperty ("name", "").toString();
        model.url = m.getProperty ("url", "").toString();
        model.sha256 = m.getProperty ("sha256", "").toString().toLowerCase();
        model.bytes = (juce::int64) m.getProperty ("bytes", 0);
        model.minMemoryGb = (int) m.getProperty ("minMemoryGb", 0);
        model.appleSilicon = (bool) m.getProperty ("appleSilicon", false);
        // a model may only come from the one model host we expect
        if ((anyModelHost || model.url.startsWith ("https://huggingface.co/")) && model.sha256.length() == 64 && model.bytes > 0) out.models.push_back (model);
    };
    const auto list = t.getProperty ("models", juce::var());
    if (auto* models = list.getArray()) for (auto& m : *models) read (m);
    if (out.models.empty()) read (t.getProperty ("model", juce::var()));          // older feeds name a single model
    if (out.models.empty()) return false;
    const auto& first = out.models.front();
    out.modelName = first.name; out.modelUrl = first.url; out.modelSha256 = first.sha256; out.modelBytes = first.bytes;
    // the helper is joined onto our own release address, so it must be a plain file name
    const bool safeFile = out.file.isNotEmpty() && out.file.containsOnly (kPlainName) && ! out.file.startsWith (".");
    return safeFile && out.version.isNotEmpty() && out.sha256.length() == 64;
}

int LocalAI::pickModel (const std::vector<TutorModel>& models, const juce::String& preferredId, bool appleSilicon, int memoryGb)
{
    if (preferredId.isNotEmpty())
        for (size_t i = 0; i < models.size(); ++i) if (models[i].id == preferredId) return (int) i;
    int best = 0;
    for (size_t i = 0; i < models.size(); ++i)
        if (memoryGb >= models[i].minMemoryGb && (appleSilicon || ! models[i].appleSilicon)) best = (int) i;       // listed lightest first
    return best;
}

void LocalAI::choose (TutorPack& pack) const
{
    auto preferred = juce::SystemStats::getEnvironmentVariable ("MU_TUTOR_MODEL", {});
    if (preferred.isEmpty()) preferred = settings->get ("tutorModel");
    const int memoryGb = juce::roundToInt ((double) juce::SystemStats::getMemorySizeInMegabytes() / 1024.0);
    const auto& m = pack.models[(size_t) pickModel (pack.models, preferred, archFolder() == "arm64", memoryGb)];
    pack.modelName = m.name; pack.modelUrl = m.url; pack.modelSha256 = m.sha256; pack.modelBytes = m.bytes;
}

void LocalAI::preferModel (const juce::String& id)
{
    settings->set ("tutorModel", id);
    lastUpgradeCheck = 0;
    refresh();
}

juce::String LocalAI::sha256Of (const juce::File& f)
{
    juce::FileInputStream in (f);
    if (! in.openedOk()) return {};
    juce::BufferedInputStream buffered (in, 1 << 20);
    return juce::SHA256 (buffered).toHexString().toLowerCase();
}

//==================================================================== install
bool LocalAI::fetch (const juce::String& url, const juce::File& target, juce::int64 expectedBytes, float from, float to, const juce::String& label, juce::String& error)
{
    juce::int64 have = target.existsAsFile() ? target.getSize() : 0;
    if (expectedBytes > 0 && have == expectedBytes) return true;
    if (expectedBytes > 0 && have > expectedBytes) { target.deleteFile(); have = 0; }

    int code = 0;
    // a part-finished download carries on where it stopped
    const auto range = have > 0 ? "Range: bytes=" + juce::String (have) + "-" : juce::String();
    auto in = juce::URL (url).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                     .withConnectionTimeoutMs (20000).withNumRedirectsToFollow (8).withStatusCode (&code).withExtraHeaders (range));
    if (in == nullptr || code >= 400) { error = "The download could not be started. Check the internet connection and try again."; return false; }
    if (have > 0 && code != 206) { target.deleteFile(); have = 0; }                                       // no partial answer: start again

    juce::FileOutputStream out (target);          // opens at the end of what is already there
    if (! out.openedOk()) { error = "The download could not be saved."; return false; }
    const auto announced = in->getTotalLength();
    const auto total = expectedBytes > 0 ? expectedBytes : (announced > 0 ? have + announced : 0);
    juce::HeapBlock<char> buffer (1 << 17);
    juce::int64 done = have;
    auto lastReport = juce::Time::getMillisecondCounter();
    while (! in->isExhausted())
    {
        if (threadShouldExit()) { error = "Stopped."; return false; }
        const int n = in->read (buffer, 1 << 17);
        if (n <= 0) break;
        if (! out.write (buffer, (size_t) n)) { error = "The disk is full."; return false; }
        done += n;
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastReport > 150)
        {
            lastReport = now;
            const float f = total > 0 ? (float) ((double) done / (double) total) : 0.0f;
            setSetup (Setup::Downloading, label + (total > 0 ? ": " + megabytes (done) + " of " + megabytes (total) : juce::String()), from + (to - from) * f);
        }
    }
    out.flush();
    if (total > 0 && done < total) { error = "The download stopped part way. Press the button again and it carries on from where it got to."; return false; }
    return done > 0;
}

bool LocalAI::unpack (const juce::File& zip, const juce::File& into)
{
    into.deleteRecursively();
    into.createDirectory();
    juce::ChildProcess tool;
   #if JUCE_MAC
    const juce::StringArray command { "/usr/bin/ditto", "-x", "-k", zip.getFullPathName(), into.getFullPathName() };
   #else
    const juce::StringArray command { "unzip", "-q", "-o", zip.getFullPathName(), "-d", into.getFullPathName() };
   #endif
    return tool.start (command, 0) && tool.waitForProcessToFinish (120000) && tool.getExitCode() == 0;
}

void LocalAI::doInstall()
{
    auto fail = [this] (const juce::String& why)
    {
        if (threadShouldExit()) setSetup (Setup::NotInstalled, {});
        else setSetup (Setup::Failed, why);
    };

    juce::String feed;
    const int feedStatus = httpStatus (feedBase + "/latest.json", 12000, &feed);
    juce::ignoreUnused (feedStatus);
    TutorPack pack;
    if (! parseTutorFeed (feed, pack, testing))
    { fail (feed.isEmpty() ? "Could not reach the download server. Check the internet connection and try again."
                           : "The AI tutor is not ready to download yet. Update the plugin first, then try again."); return; }
    choose (pack);

    const auto folder = installFolder();
    if (folder == juce::File()) { fail ("There is nowhere on this computer the tutor can be saved."); return; }

    const auto modelFile = folder.getChildFile ("model-" + pack.modelSha256.substring (0, 12) + ".gguf");
    const bool haveModel = modelFile.existsAsFile() && modelFile.getSize() == pack.modelBytes;
    const auto part = modelFile.getSiblingFile (modelFile.getFileName() + ".part");
    const auto stillNeeded = haveModel ? 0 : pack.modelBytes - (part.existsAsFile() ? part.getSize() : 0);
    if (folder.getBytesFreeOnVolume() < stillNeeded + (juce::int64) 300 * 1000 * 1000)
    { fail ("Not enough free disk space. The tutor needs about " + megabytes (pack.modelBytes + (juce::int64) 300 * 1000 * 1000) + "."); return; }

    // 1. the helper program
    juce::String error;
    const auto zip = folder.getChildFile ("helper.zip");
    zip.deleteFile();
    if (! fetch (feedBase + "/" + pack.file, zip, 0, 0.0f, 0.02f, "Downloading the tutor", error)) { zip.deleteFile(); fail (error); return; }
    if (sha256Of (zip) != pack.sha256) { zip.deleteFile(); fail ("The download was damaged on the way. Try again."); return; }
    const auto staging = folder.getChildFile ("helper-new");
    const bool unpacked = unpack (zip, staging);
    zip.deleteFile();
    const auto program = staging.getChildFile ("tutor").getChildFile (archFolder()).getChildFile ("llama-server");
    if (! unpacked || ! program.existsAsFile())
    { staging.deleteRecursively(); fail (unpacked ? "The AI tutor does not have a version for this kind of computer." : "The tutor could not be unpacked."); return; }
    stopServer();
    const auto helperFolder = folder.getChildFile ("helper");
    helperFolder.deleteRecursively();
    const bool moved = staging.getChildFile ("tutor").moveFileTo (helperFolder);
    staging.deleteRecursively();
    if (! moved) { fail ("The tutor could not be put in place."); return; }
    helperFolder.getChildFile (archFolder()).getChildFile ("llama-server").setExecutePermission (true);

    // 2. the model: one large file, resumed if a previous try was interrupted
    if (! haveModel)
    {
        if (! fetch (pack.modelUrl, part, pack.modelBytes, 0.02f, 0.97f, "Downloading the tutor", error)) { fail (error); return; }
        setSetup (Setup::Downloading, "Checking the download...", 0.98f);
        if (sha256Of (part) != pack.modelSha256) { part.deleteFile(); fail ("The download was damaged on the way. Try again."); return; }
        if (! part.moveFileTo (modelFile)) { fail ("The tutor could not be put in place."); return; }
    }
    for (const auto& e : juce::RangedDirectoryIterator (folder, false, "model-*", juce::File::findFiles))
        if (e.getFile() != modelFile) e.getFile().deleteFile();              // an older model, or leftovers

    auto* manifest = new juce::DynamicObject();
    manifest->setProperty ("version", pack.version);
    manifest->setProperty ("model", modelFile.getFileName());
    manifest->setProperty ("modelName", pack.modelName);
    manifest->setProperty ("modelBytes", pack.modelBytes);
    folder.getChildFile ("tutor.json").replaceWithText (juce::JSON::toString (juce::var (manifest)));
    {
        const juce::ScopedLock sl (lock);
        status.upgradeAvailable = false;
    }
    lastUpgradeCheck = juce::Time::currentTimeMillis();
    detect();
}

//==================================================================== the helper process
void LocalAI::stopServer()
{
    if (server.isRunning()) server.kill();
    serverPort = 0;
}

bool LocalAI::startServer (int port, bool cpuOnly)
{
    if (server.isRunning()) server.kill();
    // leave half the processor for the music software: the tutor must never make the audio stutter
    const int threads = juce::jlimit (2, 8, juce::SystemStats::getNumPhysicalCpus() / 2);
    juce::StringArray args { helperPath.getFullPathName(), "-m", modelPath.getFullPathName(), "--host", "127.0.0.1", "--port", juce::String (port),
                             "-c", "4096", "-np", "1", "-t", juce::String (threads), "-a", kAlias, "--no-webui",
                             "--sleep-idle-seconds", "240",         // gives its memory back when nobody is asking
                             "--reasoning", "off" };                // answer directly: no thinking out loud first
    if (cpuOnly) args.addArray ({ "-ngl", "0" });
    args.addTokens (extraArgs, false);
    if (! server.start (args, 0)) return false;

    for (int i = 0; i < 600 && ! threadShouldExit(); ++i)          // loading the model takes a few seconds
    {
        juce::Thread::sleep (300);
        if (httpStatus ("http://127.0.0.1:" + juce::String (port) + "/health", 1500) == 200) { serverPort = port; return true; }
        if (! server.isRunning()) return false;
    }
    server.kill();
    return false;
}

bool LocalAI::ensureServer (juce::String& error)
{
    auto ours = [] (int port)
    {
        juce::String models;
        return httpStatus ("http://127.0.0.1:" + juce::String (port) + "/v1/models", 1500, &models) == 200 && models.contains (kAlias);
    };
    if (serverPort != 0 && ours (serverPort)) return true;
    serverPort = 0;
    // another instance of the plugin (or an earlier session) may already have it running
    for (int p = kFirstPort; p < kFirstPort + 2; ++p) if (ours (p)) { serverPort = p; return true; }

    if (startServer (kFirstPort, false) || startServer (kFirstPort, true) || startServer (kFirstPort + 1, true)) return true;
    error = "The AI tutor could not start on this computer. It needs macOS 13.3 or newer and about 3 GB of free memory.";
    return false;
}

//==================================================================== questions
juce::String LocalAI::chooseModel (const juce::StringArray& installed, const juce::String& preferred)
{
    if (preferred.isNotEmpty() && installed.contains (preferred)) return preferred;
    // small general-purpose chat models first; anything is better than nothing
    for (auto* family : { "llama3.2", "gemma3", "llama3.1", "qwen", "phi", "mistral", "llama", "gemma" })
        for (auto& name : installed)
            if (name.startsWithIgnoreCase (family) && ! name.containsIgnoreCase ("embed")) return name;
    for (auto& name : installed) if (! name.containsIgnoreCase ("embed")) return name;
    return {};
}

static juce::Array<juce::var> conversationMessages (const juce::String& systemPrompt, const juce::String& question, const LocalAI::Earlier& earlier)
{
    juce::Array<juce::var> messages { message ("system", systemPrompt) };
    for (auto& turn : earlier) { messages.add (message ("user", turn.first)); messages.add (message ("assistant", turn.second)); }
    messages.add (message ("user", question));
    return messages;
}

juce::String LocalAI::buildRequest (const juce::String& model, const juce::String& systemPrompt, const juce::String& question, const Earlier& earlier)
{
    auto* options = new juce::DynamicObject();
    options->setProperty ("temperature", 0.2);
    options->setProperty ("num_predict", 320);
    auto* root = new juce::DynamicObject();
    root->setProperty ("model", model);
    root->setProperty ("stream", false);
    root->setProperty ("messages", conversationMessages (systemPrompt, question, earlier));
    root->setProperty ("options", juce::var (options));
    return juce::JSON::toString (juce::var (root), true);
}

juce::String LocalAI::buildChatRequest (const juce::String& systemPrompt, const juce::String& question, const Earlier& earlier)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("model", kAlias);
    root->setProperty ("stream", false);
    root->setProperty ("temperature", 0.2);
    root->setProperty ("max_tokens", 320);
    root->setProperty ("messages", conversationMessages (systemPrompt, question, earlier));
    return juce::JSON::toString (juce::var (root), true);
}

juce::String LocalAI::parseAnswer (const juce::String& json)
{
    const auto v = juce::JSON::parse (json);
    auto text = v.getProperty ("message", juce::var()).getProperty ("content", "").toString();              // Ollama
    if (text.isEmpty())
        if (auto* choices = v.getProperty ("choices", juce::var()).getArray())                               // the built-in helper
            if (! choices->isEmpty()) text = choices->getReference (0).getProperty ("message", juce::var()).getProperty ("content", "").toString();
    // some models wrap their private reasoning in tags; students only need the answer
    while (text.contains ("<think>") && text.contains ("</think>"))
        text = text.upToFirstOccurrenceOf ("<think>", false, false) + text.fromFirstOccurrenceOf ("</think>", false, false);
    // the answer is shown as plain text, so drop the markdown small models like to add
    juce::StringArray lines;
    lines.addLines (text.replace ("**", ""));
    for (auto& l : lines)
    {
        auto t = l.trimStart();
        while (t.startsWithChar ('#')) t = t.substring (1);
        if (t.startsWith ("* ")) t = "- " + t.substring (2);
        l = t.trim();
    }
    return lines.joinIntoString ("\n").trim();
}

void LocalAI::detect()
{
    Installed have;
    const bool builtIn = findInstalled (have);
    readOffer (builtIn ? &have : nullptr);
    const auto helper = have.helper, model = have.model;
    const auto builtInName = have.modelName;

    juce::String ollamaModel;
    bool ollamaEmpty = false;
    if (! builtIn)
    {
        juce::String body;
        juce::StringArray names;
        if (httpStatus (address + "/api/tags", 1500, &body) != 0)
        {
            const auto tags = juce::JSON::parse (body);
            if (auto* models = tags.getProperty ("models", juce::var()).getArray())
                for (auto& m : *models) names.add (m.getProperty ("name", "").toString());
        }
        ollamaModel = chooseModel (names, settings->get ("aiModel"));
        ollamaEmpty = names.isEmpty();
    }

    const juce::ScopedLock sl (lock);
    helperPath = helper; modelPath = model;
    status.builtIn = builtIn;
    status.available = builtIn || ollamaModel.isNotEmpty();
    status.model = builtIn ? builtInName : ollamaModel;
    if (! builtIn) status.upgradeAvailable = false;
    if (builtIn) { status.setup = Setup::Ready; status.setupMessage.clear(); status.progress = 1.0f; }
    else if (status.setup == Setup::Ready || status.setup == Setup::Downloading) { status.setup = Setup::NotInstalled; status.setupMessage.clear(); }
    if (! status.available) status.error = ollamaEmpty ? "" : "Ollama is running but no chat model is installed.";
}

void LocalAI::doAsk()
{
    if (! getStatus().available) detect();
    juce::String model, system, question;
    bool builtIn = false;
    Earlier earlier;
    {
        const juce::ScopedLock sl (lock);
        model = status.model; system = pendingSystem; question = pendingQuestion; builtIn = status.builtIn; earlier = history;
    }
    juce::String answer, error;
    if (model.isEmpty()) error = "The AI tutor is not turned on. Open Settings to turn it on.";
    else if (builtIn)
    {
        if (ensureServer (error))
        {
            int code = 0;
           #if JUCE_MAC
            const juce::String headers ("Content-Type: application/json");
           #else
            const juce::String headers;       // JUCE's Linux HTTP code (tests only) corrupts a POST that has extra headers
           #endif
            const auto url = juce::URL ("http://127.0.0.1:" + juce::String (serverPort) + "/v1/chat/completions").withPOSTData (buildChatRequest (system, question, earlier));
            if (auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                                     .withConnectionTimeoutMs (240000).withStatusCode (&code).withExtraHeaders (headers)))
                answer = parseAnswer (in->readEntireStreamAsString());
            if (answer.isEmpty()) error = "The AI tutor did not answer. Try asking again.";
        }
    }
    else
    {
        int code = 0;
        const auto url = juce::URL (address + "/api/chat").withPOSTData (buildRequest (model, system, question, earlier));
        // No extra headers: Ollama reads the body as JSON whatever the content type says (its own curl examples send none).
        auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                             .withConnectionTimeoutMs (120000).withStatusCode (&code));
        if (in != nullptr) answer = parseAnswer (in->readEntireStreamAsString());
        if (answer.isEmpty()) error = "The AI tutor did not answer. Check that Ollama is still running.";
    }
    const juce::ScopedLock sl (lock);
    if (answer.isNotEmpty())
    {
        history.push_back ({ question, answer });
        if (history.size() > 2) history.erase (history.begin());
    }
    status.busy = false; status.answer = answer; status.error = error; ++status.answerId;
}
}
