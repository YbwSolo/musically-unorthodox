#include "Icons.h"
#include "BinaryData.h"
#include <map>

namespace mu::icons
{
const juce::Path& get (const juce::String& name)
{
    static std::map<juce::String, juce::Path> cache;
    auto it = cache.find (name);
    if (it != cache.end()) return it->second;

    juce::Path path;
    int size = 0;
    if (const char* data = BinaryData::getNamedResource ((name + "_svg").toRawUTF8(), size))
    {
        if (auto xml = juce::parseXML (juce::String::fromUTF8 (data, size)))
        {
            // every icon starts with a transparent full-size rect that only sets the canvas; it is not part of the shape
            for (auto* child = xml->getFirstChildElement(); child != nullptr;)
            {
                auto* next = child->getNextElement();
                if (child->hasTagName ("rect") && child->getStringAttribute ("fill") == "none") xml->removeChildElement (child, true);
                child = next;
            }
            if (auto drawable = juce::Drawable::createFromSVG (*xml))
                path = drawable->getOutlineAsPath();
        }
    }
    return cache.emplace (name, path).first->second;
}

void draw (juce::Graphics& g, const juce::String& name, juce::Rectangle<float> area, juce::Colour colour)
{
    const auto& p = get (name);
    if (p.isEmpty()) return;
    const float side = juce::jmin (area.getWidth(), area.getHeight());
    const auto box = area.withSizeKeepingCentre (side, side);
    g.setColour (colour);
    g.fillPath (p, juce::AffineTransform::scale (side / 256.0f).translated (box.getX(), box.getY()));
}
}
