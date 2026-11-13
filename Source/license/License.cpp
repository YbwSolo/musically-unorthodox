#include "License.h"
#include <juce_cryptography/juce_cryptography.h>

namespace mu
{
static const char* kSecret = "musically-unorthodox-2026-mix-assistant";

License& License::get()
{
    static License instance;
    return instance;
}

License::License() { load(); }

juce::File License::getLicenseFile() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Musically Unorthodox");
    dir.createDirectory();
    return dir.getChildFile ("license.xml");
}

juce::String License::makeKeyForEmail (const juce::String& emailIn)
{
    const auto email = emailIn.trim().toLowerCase();
    const juce::SHA256 sha ((email + "|" + kSecret).toUTF8());
    const auto hex = sha.toHexString().toUpperCase();
    juce::String k ("MU");
    for (int i = 0; i < 4; ++i) k << "-" << hex.substring (i * 4, i * 4 + 4);
    return k;
}

bool License::isFeatureFree (const juce::String& f)
{
    return f == features::sessionBalance || f == features::levelsPage || f == features::manualEQ || f == features::manualComp;
}

juce::String License::getEmail() const { const juce::ScopedLock sl (lock); return email; }
juce::String License::getKey() const { const juce::ScopedLock sl (lock); return key; }

bool License::activate (const juce::String& e, const juce::String& k)
{
    const auto cleaned = k.trim().toUpperCase().removeCharacters (" ");
    if (cleaned != makeKeyForEmail (e)) return false;
    {
        const juce::ScopedLock sl (lock);
        email = e.trim(); key = cleaned;
    }
    pro.store (true);
    save();
    return true;
}

void License::deactivate()
{
    { const juce::ScopedLock sl (lock); email.clear(); key.clear(); }
    pro.store (false);
    getLicenseFile().deleteFile();
}

void License::load()
{
    auto f = getLicenseFile();
    if (! f.existsAsFile()) return;
    if (auto xml = juce::XmlDocument::parse (f))
    {
        const auto e = xml->getStringAttribute ("email");
        const auto k = xml->getStringAttribute ("key");
        if (k == makeKeyForEmail (e))
        {
            const juce::ScopedLock sl (lock);
            email = e; key = k;
            pro.store (true);
        }
    }
}

void License::save()
{
    juce::XmlElement xml ("MULicense");
    { const juce::ScopedLock sl (lock); xml.setAttribute ("email", email); xml.setAttribute ("key", key); }
    xml.writeTo (getLicenseFile());
}
}
